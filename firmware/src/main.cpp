#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "temp_sensor.h"
#include "ph_sensor.h"
#include "do_sensor.h"
#include "turbidity_sensor.h"
#include "dht_sensor.h"
#include "float_switch.h"
#include "sensor_data.h"
#include "rule_engine.h"
#include "lcd_display.h"
#include "firestore_client.h"
#include "time_utils.h"


// === CONSTANTS ===
#define USE_DHT_MOCK true
#define USE_PH_MOCK true
#define USE_DO_MOCK true
#define USE_TURBIDITY_MOCK true
#define USE_WATERTEMP_MOCK true
#define USE_FLOATSWITCH_MOCK true


RealTimeData current = {}; // Initialize all fields to 0/false
ActuatorState actuators = {}; // Initialize all actuator states to false/auto
Commands currentCommands = { 
    {true, false}, // fan
    {true, false}, // light
    {true, false}, // pump
    {true, false}  // valve
}; 

// Flag to trigger rule application when commands change via stream
volatile bool commandsChangedViaStream = false;
unsigned long lastStreamUpdate = 0;


// === FORWARD DECLARATIONS ===
void initAllModules();
bool readSensors(unsigned long now, RealTimeData &data);
void displaySensorReading(const char* label, float value, const char* unit) {
    Serial.printf("[SENSOR] %s = %.2f %s\n", label, value, unit);
}
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected.");
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("🚀 AquaBell System Starting...");

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    initAllModules();

    // === WiFi Connection ===
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
        delay(500);
        yield(); // prevent watchdog reset
        Serial.print(".");
        wifiAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {    
        Serial.println(" connected.");

        firebaseSignIn();

        // Start Firebase RTDB stream for real-time commands
        Serial.println("🔥 Starting Firebase RTDB stream...");
        startFirebaseStream();

        // Time sync now that WiFi is connected
        Serial.println("🕐 Initializing NTP time sync...");
        bool timeSyncSuccess = syncTimeOncePerBoot(10000); // shorter timeout

        if (timeSyncSuccess) {
            Serial.println("✅ Time sync successful - schedule-based actions enabled");
        } else {
            Serial.println("⚠️ Time sync failed - schedule-based actions suspended until time available");
        }
    } else {
        Serial.println(" failed. Continuing without WiFi.");
    }
    Serial.println("✅ System initialization complete.");
}


// === LOOP ===
void loop() {
    unsigned long nowMillis = millis();

    // Handle WiFi reconnect logic (no change)
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWiFiAttempt = 0;
        if (nowMillis - lastWiFiAttempt >= 30000) {
            Serial.println("📡 WiFi reconnecting...");
            WiFi.reconnect();
            lastWiFiAttempt = nowMillis;
        }
    }

    periodicTimeSync();

    // Handle Firebase stream updates if WiFi available
    if (WiFi.status() == WL_CONNECTED) {
        handleFirebaseStream();
    }

    bool sensorsUpdated = readSensors(nowMillis, current);

    if (sensorsUpdated || commandsChangedViaStream || is_float_switch_triggered()) {
        lcd_display(current);

        bool wifiUp = (WiFi.status() == WL_CONNECTED);
        bool fbReady = isFirebaseReady();
        bool cmdsSynced = isInitialCommandsSynced();

        // 🧱 Gate logic — skip automation until Firebase fully synced
        if (wifiUp && (!fbReady || !cmdsSynced)) {
            Serial.println("[MAIN] Firebase not ready — holding actuators OFF");
            actuators.fan = false;
            actuators.light = false;
            actuators.pump = false;
            actuators.valve = false;
            commandsChangedViaStream = false;
            goto POST_RULES;
        }

        // ✅ Safe zone: Firebase ready and /commands synced
        if (wifiUp) {
            applyRulesWithModeControl(current, actuators, currentCommands, nowMillis);
        } else {
            // Offline fallback (AUTO-all)
            Commands defaultAuto = { {true,false}, {true,false}, {true,false}, {true,false} };
            applyRulesWithModeControl(current, actuators, defaultAuto, nowMillis);
        }

        // Reflect current relay states
        current.relayStates.fan       = actuators.fan;
        current.relayStates.light     = actuators.light;
        current.relayStates.waterPump = actuators.pump;
        current.relayStates.valve     = actuators.valve;

        // ✅ Only sync after Firebase and commands are ready
        if (wifiUp && fbReady && cmdsSynced) {
        // 🧠 Prevent immediate echo after RTDB stream change
        if (millis() - lastStreamUpdate > 2000) {
            syncRelayState(current, currentCommands);
            pushToFirestoreLive(current);
        } else {
            Serial.println("[MAIN] Skipping sync — too soon after stream update");
        }
}

POST_RULES:
        commandsChangedViaStream = false;
    }

    if (isRetryInProgress()) processRetry();

    // --- Batch logging block (no change) ---
    static unsigned long lastBatchLog = 0;
    const unsigned long batchInterval = 600000;
    const int batchSize = 60;
    static RealTimeData logBuffer[batchSize];
    static int logIndex = 0;

    if (WiFi.status() == WL_CONNECTED) {
        if (sensorsUpdated && logIndex < batchSize) logBuffer[logIndex++] = current;
        if ((nowMillis - lastBatchLog >= batchInterval) || (logIndex >= batchSize)) {
            time_t timestamp = getUnixTime();
            if (timestamp > 0 && logIndex > 0) {
                pushBatchLogToFirestore(logBuffer, logIndex, timestamp);
            }
            logIndex = 0;
            lastBatchLog = nowMillis;
        }
    }

    yield();
}


// === INIT HELPERS ===
void initAllModules() {
    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();
    relay_control_init(); 
    lcd_init();
    Serial.println("✅ All modules initialized successfully.");
}
// === UNIFIED SENSOR READING (Every 10 seconds) ===
bool readSensors(unsigned long now, RealTimeData &data) {
    static unsigned long lastSensorRead = 0;
    static bool lastFloatState = false;

    if (now - lastSensorRead < UNIFIED_SENSOR_INTERVAL) {
        return false; 
    }

    Serial.println("📊 Reading all sensors...");
    bool updated = false;

    // --- Water Temp ---
    float temp = USE_WATERTEMP_MOCK ? 25.0 : read_waterTemp();
    
    if (!isnan(temp) && temp > -50.0f && temp < 100.0f) {
        data.waterTemp = temp;
        // displaySensorReading("🌡️ Water Temp", data.waterTemp, "°C");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid water temperature");
    }

    // --- pH ---
    float ph = USE_PH_MOCK ? 7.0 : read_ph(data.waterTemp);
    
    if (!isnan(ph) && ph > -2.0f && ph < 16.0f) {
        data.pH = ph;
        // displaySensorReading("🧪 pH", data.pH, "");
        updated = true;
    } else {
        static unsigned long lastPHError = 0;
        if (now - lastPHError >= 60000) { // Log error only once per minute
            Serial.println("⚠️ Invalid pH");
            lastPHError = now;
        }
    }

    // --- Dissolved Oxygen ---
    float doValue = USE_DO_MOCK ? 6.5 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
    
    if (!isnan(doValue) && doValue >= -5.0f && doValue < 25.0f) {
        data.dissolvedOxygen = doValue;
        // displaySensorReading("🫧 DO", data.dissolvedOxygen, "mg/L");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid DO");
    }

    // --- Turbidity ---
    float turbidity = USE_TURBIDITY_MOCK ? 20.0 : read_turbidity();
    
    if (!isnan(turbidity) && turbidity >= -100.0f && turbidity < 3000.0f) {
        data.turbidityNTU = turbidity;
        // displaySensorReading("🌊 Turbidity", data.turbidityNTU, "NTU");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid turbidity");
    }

    // --- DHT (Air Temp & Humidity) ---
    float airTemp, airHumidity;
    if (USE_DHT_MOCK) {
        airTemp = 27.0;
        airHumidity = 65.0;
    } else {
        airTemp = read_dhtTemp();
        airHumidity = read_dhtHumidity();
    }
    
    if (!isnan(airTemp) && !isnan(airHumidity) && 
        airTemp > -40.0f && airTemp < 80.0f &&
        airHumidity >= 0.0f && airHumidity <= 100.0f) {
        data.airTemp = airTemp;
        data.airHumidity = airHumidity;
        // displaySensorReading("🌡️ Air Temp", data.airTemp, "°C");
        // displaySensorReading("💨 Air Humidity", data.airHumidity, "%");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid DHT");
    }

    // --- Float Switch ---
    bool floatState = USE_FLOATSWITCH_MOCK ? false : float_switch_active();

    if (floatState != lastFloatState) {
        data.floatTriggered = floatState;
        Serial.printf("💧 Float Switch: %s\n", floatState ? "TRIGGERED" : "NORMAL");
        lastFloatState = floatState;
        updated = true;
    }

    // Update last read time
    lastSensorRead = now;
    
    if (updated) {
        Serial.println("✅ All sensors read successfully");
    }
    
    return updated;
}
