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
#include "firebase.h"
#include "time_utils.h"

// === CONSTANTS ===
#define USE_DHT_MOCK false
#define USE_PH_MOCK true
#define USE_DO_MOCK true
#define USE_TURBIDITY_MOCK true
#define USE_WATERTEMP_MOCK false
#define USE_FLOATSWITCH_MOCK false

// === GLOBAL STATES ===
RealTimeData current = {};
ActuatorState actuators = {};

Commands currentCommands = {
    {true, false}, // fan
    {true, false}, // light
    {true, false}, // pump
    {true, false}  // valve
};

// === FLAGS & TIMERS ===
volatile bool commandsChangedViaStream = false;
unsigned long lastStreamUpdate = 0;
unsigned long lastRelaySync = 0;

// === FORWARD DECLARATIONS ===
void initAllModules();
bool readSensors(unsigned long now, RealTimeData &data);
void connectWiFi();

// === UTILS ===
void displaySensorReading(const char* label, float value, const char* unit) {
    Serial.printf("[SENSOR] %s = %.2f %s\n", label, value, unit);
}

void connectWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        yield();
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" connected.");
    } else {
        Serial.println(" failed. Continuing without WiFi.");
    }
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("🚀 AquaBell System Starting...");
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    initAllModules();
    connectWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        firebaseSignIn();

        Serial.println("🔥 Starting Firebase RTDB stream...");
        startFirebaseStream();

        Serial.println("🕐 Initializing NTP time sync...");
        bool timeSyncSuccess = syncTimeOncePerBoot(10000);
        if (timeSyncSuccess) {
            Serial.println("✅ Time sync successful - schedule-based actions enabled");
        } else {
            Serial.println("⚠️ Time sync failed - schedule-based actions suspended until time available");
        }
    }

    Serial.println("✅ System initialization complete.");
}

// === MAIN LOOP ===
void loop() {
    unsigned long nowMillis = millis();

    // === Handle WiFi reconnect ===
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWiFiAttempt = 0;
        if (nowMillis - lastWiFiAttempt >= 30000) {
            Serial.println("📡 WiFi reconnecting...");
            WiFi.reconnect();
            lastWiFiAttempt = nowMillis;
        }
    }

    // === Refresh Firebase token periodically ===
    if (WiFi.status() == WL_CONNECTED) {
        safeTokenRefresh(); // ✅ Non-blocking, auto-handles expiry and re-sign-in
    }

    // === Periodic NTP Time Sync ===
    periodicTimeSync();

    // === Handle Firebase Stream ===
    if (WiFi.status() == WL_CONNECTED) {
        handleFirebaseStream();
    }

    // === Read Sensors ===
    bool sensorsUpdated = readSensors(nowMillis, current);

    if (sensorsUpdated || commandsChangedViaStream || is_float_switch_triggered()) {
        lcd_display(current);

        bool wifiUp = (WiFi.status() == WL_CONNECTED);
        bool fbReady = isFirebaseReady();
        bool cmdsSynced = isInitialCommandsSynced();

        // === Gate logic: wait until Firebase ready ===
        if (wifiUp && (!fbReady || !cmdsSynced)) {
            Serial.println("[MAIN] Firebase not ready — holding actuators OFF");
            actuators.fan = false;
            actuators.light = false;
            actuators.pump = false;
            actuators.valve = false;
            commandsChangedViaStream = false;
        } else {
            // === Automation & Manual Control ===
            if (wifiUp) {
                applyRulesWithModeControl(current, actuators, currentCommands, nowMillis);
            } else {
                Commands defaultAuto = { {true,false}, {true,false}, {true,false}, {true,false} };
                applyRulesWithModeControl(current, actuators, defaultAuto, nowMillis);
            }

            // === Reflect relay states ===
            current.relayStates.fan       = actuators.fan;
            current.relayStates.light     = actuators.light;
            current.relayStates.waterPump = actuators.pump;
            current.relayStates.valve     = actuators.valve;

            // === Sync to Firebase if ready ===
            if (wifiUp && fbReady && cmdsSynced) {
                bool debounceExpired = (millis() - lastStreamUpdate > 5000);
                bool cooldownExpired = (millis() - lastRelaySync > RELAY_SYNC_COOLDOWN);

                if (debounceExpired && cooldownExpired) {
                    syncRelayState(current, currentCommands);
                    pushToRTDBLive(current);
                    lastRelaySync = millis();
                } else {
                    Serial.println("[MAIN] Skipping sync — debounce/cooldown active");
                }
            }

            commandsChangedViaStream = false;
        }
    }

    // === Retry Handling ===
    if (isRetryInProgress()) processRetry();

    // === Batch Firestore Logging ===
    static RealTimeData logBuffer[60];
    static int logIndex = 0;
    static unsigned long lastBatchLog = 0;
    const unsigned long batchInterval = 600000;

    if (WiFi.status() == WL_CONNECTED) {
        if (sensorsUpdated && logIndex < 60)
            logBuffer[logIndex++] = current;

        bool intervalElapsed = (nowMillis - lastBatchLog >= batchInterval);
        bool bufferFull = (logIndex >= 60);

        if ((intervalElapsed || bufferFull) && logIndex > 0) {
            time_t timestamp = getUnixTime();
            if (timestamp > 0) {
                bool success = pushBatchLogToFirestore(logBuffer, logIndex, timestamp);
                if (success) {
                    logIndex = 0; // ✅ Only clear after success
                } else {
                    Serial.println("[Firestore] Retaining batch for next retry");
                }
            }
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

bool readSensors(unsigned long now, RealTimeData &data) {
    static unsigned long lastSensorRead = 0;
    static bool lastFloatState = false;

    if (now - lastSensorRead < UNIFIED_SENSOR_INTERVAL) {
        return false; 
    }

    bool updated = false;

    // --- Water Temp (°C) ---
    float temp = USE_WATERTEMP_MOCK ? 28.5 : read_waterTemp();
    
    if (!isnan(temp) && temp > -50.0f && temp < 100.0f) {
        data.waterTemp = temp;
        updated = true;
    } else {
        Serial.println("⚠️ Invalid water temperature");
    }

    // --- pH ---
    float ph = USE_PH_MOCK ? 7.2 : read_ph(data.waterTemp);
    
    if (!isnan(ph) && ph > -2.0f && ph < 16.0f) {
        data.pH = ph;
        updated = true;
    } else {
        static unsigned long lastPHError = 0;
        if (now - lastPHError >= 60000) {
            Serial.println("⚠️ Invalid pH");
            lastPHError = now;
        }
    }

    // --- Dissolved Oxygen (mg/L) ---
    float doValue = USE_DO_MOCK ? 7.5 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
    
    if (!isnan(doValue) && doValue >= -5.0f && doValue < 25.0f) {
        data.dissolvedOxygen = doValue;
        updated = true;
    } else {
        Serial.println("⚠️ Invalid DO");
    }

    // --- Turbidity (NTU) ---
    float turbidity = USE_TURBIDITY_MOCK ? 50.0 : read_turbidity();
    
    if (!isnan(turbidity) && turbidity >= -100.0f && turbidity < 3000.0f) {
        data.turbidityNTU = turbidity;
        updated = true;
    } else {
        Serial.println("⚠️ Invalid turbidity");
    }

    // --- DHT (Air Temp °C & Humidity %) ---
    float airTemp, airHumidity;
    if (USE_DHT_MOCK) {
        airTemp = 27.0;
        airHumidity = 60.0;
    } else {
        airTemp = read_dhtTemp();
        airHumidity = read_dhtHumidity();
    }
    
    if (!isnan(airTemp) && !isnan(airHumidity) && 
        airTemp > -40.0f && airTemp < 80.0f &&
        airHumidity >= 0.0f && airHumidity <= 100.0f) {
        data.airTemp = airTemp;
        data.airHumidity = airHumidity;
        updated = true;
    } else {
        Serial.println("⚠️ Invalid DHT");
    }

    // --- Float Switch ---
    bool floatState = USE_FLOATSWITCH_MOCK 
        ? false 
        : float_switch_active();

    if (floatState != lastFloatState) {
        data.floatTriggered = floatState;
        Serial.printf("💧 Float Switch: %s\n", floatState ? "TRIGGERED" : "NORMAL");
        lastFloatState = floatState;
        updated = true;
    }

    lastSensorRead = now;

    if (updated) {
        Serial.println("✅ All sensors read successfully");
    }

    return updated;
}