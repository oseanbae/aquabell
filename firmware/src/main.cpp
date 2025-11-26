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
#define USE_TURBIDITY_MOCK false
#define USE_WATERTEMP_MOCK false
#define USE_FLOATSWITCH_MOCK false

// === GLOBAL STATES ===
RealTimeData current = {};
ActuatorState actuators = {};

Commands currentCommands = {
    {true, false}, // fan
    {true, false}, // light
    {true, false}, // pump
    {true, false}, // valve
    {true, false}, // cooler
    {true, false}, // heater
    {true, false}  // pH dosing (enabled flag, current activity)
};

// === FLAGS & TIMERS ===
volatile bool commandsChangedViaStream = false;
unsigned long lastStreamUpdate = 0;
unsigned long lastRelaySync = 0;

// === FORWARD DECLARATIONS ===
void initAllModules();
bool readSensorsMultiInterval(unsigned long now, RealTimeData &data, bool updatedSensors[5]);
void connectWiFi();
float read_ph_mock();
float read_do_mock();

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
    bool updatedSensors[5] = {false, false, false, false, false}; // waterTemp, pH, DO, turbidity, airTemp/humidity
    bool sensorsUpdated = readSensorsMultiInterval(nowMillis, current, updatedSensors);

    // Push to RTDB if any sensor updated
    if (sensorsUpdated) {
        pushToRTDBLive(current, updatedSensors);
        lcd_display(current); // only update display if something changed
    }
    
    bool floatEvent = is_float_switch_triggered(); // check float switch state
    bool currentFloatState  = float_switch_active();  // get current float switch state
    current.floatTriggered = currentFloatState; // update data struct

    // ===== pH PUMP LOGIC - Always runs independently, no Firebase dependency =====
    // Must run every loop iteration for non-blocking timing control
    checkpHPumpLogic(actuators, current.pH, nowMillis);
    current.relayStates.phRaising = actuators.phRaising;
    current.relayStates.phLowering = actuators.phLowering;

    // Update LCD
    if (sensorsUpdated || commandsChangedViaStream || floatEvent) {
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
            actuators.cooler = false;
            actuators.heater = false;
            commandsChangedViaStream = false;
        } else {
            // === Automation & Manual Control ===
            if (wifiUp) {
                applyRulesWithModeControl(current, actuators, currentCommands, nowMillis);
            } else {
                Commands defaultAuto = {
                    {true,false}, {true,false}, {true,false},
                    {true,false}, {true,false}, {true,false},
                    {true,false}
                };
                applyRulesWithModeControl(current, actuators, defaultAuto, nowMillis);
            }

            // === Reflect relay states ===
            current.relayStates.fan       = actuators.fan;
            current.relayStates.light     = actuators.light;
            current.relayStates.waterPump = actuators.pump;
            current.relayStates.valve     = actuators.valve;
            current.relayStates.cooler = actuators.cooler;
            current.relayStates.heater = actuators.heater;
            // pH pump states already reflected above (runs independently)

            // === Sync to Firebase if ready ===
            if (wifiUp && fbReady && cmdsSynced) {
                bool debounceExpired = (millis() - lastStreamUpdate > 5000);
                bool cooldownExpired = (millis() - lastRelaySync > RELAY_SYNC_COOLDOWN);

                if (debounceExpired && cooldownExpired) {
                    syncRelayState(current, currentCommands);
                    // Use empty array if no sensors updated (for relay state sync only)
                    bool emptySensors[6] = {false, false, false, false, false, false};
                    pushToRTDBLive(current, emptySensors);
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

bool readSensorsMultiInterval(unsigned long now, RealTimeData &data, bool updatedSensors[5]) {
    static unsigned long lastRead[5] = {0, 0, 0, 0, 0};
    const unsigned long intervals[5] = {
        10000,   // waterTemp: 10s
        60000,   // pH: 1min
        15000,   // dissolvedOxygen: 15s
        30000,   // turbidity: 30s
        60000    // airTemp & airHumidity: 60s
    };

    bool updated = false;
    for (int i = 0; i < 5; i++) updatedSensors[i] = false;

    // --- Water Temp ---
    if (now - lastRead[0] >= intervals[0]) {
        float temp = USE_WATERTEMP_MOCK ? 28.5 : read_waterTemp();
        if (!isnan(temp) && temp > -50.0f && temp < 100.0f) {
            data.waterTemp = temp;
            updatedSensors[0] = true;
            updated = true;
        }
        lastRead[0] = now;
    }

    // --- pH ---
    if (now - lastRead[1] >= intervals[1]) {
        float ph = USE_PH_MOCK ? read_ph_mock(): read_ph(data.waterTemp);
        if (!isnan(ph) && ph > -2.0f && ph < 16.0f) {
            data.pH = ph;
            updatedSensors[1] = true;
            updated = true;
        }
        lastRead[1] = now;
    }

    // --- Dissolved Oxygen ---
    if (now - lastRead[2] >= intervals[2]) {
        float doValue = USE_DO_MOCK ? read_do_mock() : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
        if (!isnan(doValue) && doValue >= -5.0f && doValue < 25.0f) {
            data.dissolvedOxygen = doValue;
            updatedSensors[2] = true;
            updated = true;
        }
        lastRead[2] = now;
    }

    // --- Turbidity ---
    if (now - lastRead[3] >= intervals[3]) {
        float turbidity = USE_TURBIDITY_MOCK ? 50.0 : read_turbidity();
        if (!isnan(turbidity) && turbidity >= -100.0f && turbidity < 3000.0f) {
            data.turbidityNTU = turbidity;
            updatedSensors[3] = true;
            updated = true;
        }
        lastRead[3] = now;
    }

    // --- Air Temp & Humidity ---
    if (now - lastRead[4] >= intervals[4]) {
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
            updatedSensors[4] = true;
            updated = true;
        }
        lastRead[4] = now;
    }

    return updated;
}


float read_ph_mock() {
    static float mockPH = 7.2f;
    static unsigned long lastUpdate = 0;

    unsigned long now = millis();
    if (now - lastUpdate > 45000) {  // update every 30 s
        float drift = ((float)random(-2, 3)) / 1000.0f;  // -0.002 to +0.003
        mockPH += drift;

        if (mockPH < 6.3f) mockPH = 6.3f;
        if (mockPH > 7.8f) mockPH = 7.8f;

        lastUpdate = now;
    }
    return mockPH;
}

float read_do_mock() {
    static float mockDO = 6.5f;  // Start midrange (mg/L)
    static unsigned long lastUpdate = 0;

    unsigned long now = millis();
    if (now - lastUpdate > 30000) {  // update every 30s
        // Small random drift up/down
        float drift = ((float)random(-10, 11)) / 100.0f;  // -0.10 to +0.10
        mockDO += drift;

        // Clamp within realistic tilapia range (5.0–8.0 mg/L)
        if (mockDO < 5.0f) mockDO = 5.0f;
        if (mockDO > 8.0f) mockDO = 8.0f;

        lastUpdate = now;
    }
    return mockDO;
}