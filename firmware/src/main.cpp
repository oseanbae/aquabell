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
#include "firestore_client.h"
#include "time_utils.h"

// === CONSTANTS ===
#define USE_MOCK_DATA true

RealTimeData current = {}; // Initialize all fields to 0/false

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

    // Initialize NTP time sync
    Serial.println("🕐 Initializing NTP time sync...");
    bool timeSyncSuccess = syncTimeOncePerBoot(20000); // 20 second timeout
    
    if (timeSyncSuccess) {
        Serial.println("✅ Time sync successful - schedule-based actions enabled");
    } else {
        Serial.println("⚠️ Time sync failed - schedule-based actions suspended until time available");
    }

    // Non-blocking WiFi connection
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi...");
    
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
        delay(500);
        Serial.print(".");
        wifiAttempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" connected.");
        firebaseSignIn();
        
        // If time sync failed during setup, try again now that WiFi is connected
        if (!timeSyncSuccess) {
            Serial.println("🕐 Retrying time sync now that WiFi is connected...");
            timeSyncSuccess = syncTimeOncePerBoot(10000); // Shorter timeout for retry
            if (timeSyncSuccess) {
                Serial.println("✅ Time sync successful on retry - schedule-based actions enabled");
            }
        }
    } else {
        Serial.println(" failed. Continuing without WiFi.");
    }

    initAllModules();
    
    Serial.println("✅ System initialization complete.");
}

// === LOOP ===
void loop() {
    unsigned long nowMillis = millis();

    // Non-blocking WiFi check
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWiFiAttempt = 0;
        if (nowMillis - lastWiFiAttempt >= 30000) { // Try every 30 seconds
            Serial.println("📡 WiFi reconnecting...");
            WiFi.reconnect();
            lastWiFiAttempt = nowMillis;
        }
    }
    
    // Periodic time sync (every 24 hours when WiFi is available)
    periodicTimeSync();

    // Read sensors
    bool updated = readSensors(nowMillis, current);
    
    // ← ENABLE RULE ENGINE
    if (updated || is_float_switch_triggered()) {
        // Only apply time-dependent rules if time is available
        if (isTimeAvailable()) {
            struct tm timeinfo;
            if (getLocalTm(timeinfo)) {
                apply_rules(current, timeinfo);
            }
        } else {
            // Apply emergency rules only (float switch, basic safety)
            apply_emergency_rules(current);
        }
    }

    // Firebase updates (only if WiFi is connected)
    static unsigned long lastLiveUpdate = 0;
    static unsigned long lastBatchLog = 0;
    const unsigned long liveInterval = 10000;
    const unsigned long batchInterval = 600000;
    const int batchSize = 60;
    static RealTimeData logBuffer[batchSize];
    static int logIndex = 0;

    if (WiFi.status() == WL_CONNECTED) {
        // Live push stays at a fixed 10s interval (for the app UI)
        if (nowMillis - lastLiveUpdate >= liveInterval) {
            pushToFirestoreLive(current);
            lastLiveUpdate = nowMillis;
        }

        // Append to logBuffer only when fresh sensor data is available
        if (updated) {  
            logBuffer[logIndex++] = current;
        }

        // Every 10 min OR if buffer is full → push batch
        if ((nowMillis - lastBatchLog >= batchInterval) || (logIndex >= batchSize)) {
            time_t timestamp = getUnixTime();
            if (timestamp > 0) {
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
    Serial.println("✅ All modules initialized successfully.");
}

// === IMPROVED SENSOR READING ===
bool readSensors(unsigned long now, RealTimeData &data) {
    static unsigned long lastTempRead = 0, lastPHRead = 0, lastDORead = 0;
    static unsigned long lastTurbidityRead = 0, lastDHTRead = 0, lastFloatRead = 0;
    static bool lastFloatState = false;

    bool updated = false;
    // ← FIXED: Don't overwrite data unnecessarily
    // data = current; // REMOVED THIS LINE

    // --- Water Temp ---
    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        float temp = USE_MOCK_DATA ? 
            25.0 + (rand() % 100) / 10.0 : read_waterTemp();
        
        // ← ADDED: Validation
        if (!isnan(temp) && temp > -50.0f && temp < 100.0f) {
            data.waterTemp = temp;
            displaySensorReading("🌡️ Water Temp", data.waterTemp, "°C");
            lastTempRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid water temperature");
        }
    }

    // --- pH ---
    if (now - lastPHRead >= PH_READ_INTERVAL) {
        float ph = USE_MOCK_DATA ? 
            6.5 + (rand() % 50) / 10.0 : read_ph(data.waterTemp);
        
        // More lenient pH validation for testing (allow wider range)
        if (!isnan(ph) && ph > -2.0f && ph < 16.0f) {
            data.pH = ph;
            displaySensorReading("🧪 pH", data.pH, "");
            lastPHRead = now;
            updated = true;
        } else {
            // Only log pH errors occasionally to reduce spam
            static unsigned long lastPHError = 0;
            if (now - lastPHError >= 60000) { // Log error only once per minute
                Serial.println("⚠️ Invalid pH (sensor may not be connected)");
                lastPHError = now;
            }
        }
    }

    // --- Dissolved Oxygen ---
    if (now - lastDORead >= DO_READ_INTERVAL) {
        float doValue = USE_MOCK_DATA ? 
            5.0 + (rand() % 40) / 10.0 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
        
        if (!isnan(doValue) && doValue >= -5.0f && doValue < 25.0f) { // More lenient for testing
            data.dissolvedOxygen = doValue;
            displaySensorReading("🫧 DO", data.dissolvedOxygen, "mg/L");
            lastDORead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid DO");
        }
    }

    // --- Turbidity ---
    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        float turbidity = USE_MOCK_DATA ? 
            (rand() % 1000) / 10.0 : read_turbidity();
        
        if (!isnan(turbidity) && turbidity >= -100.0f && turbidity < 3000.0f) { // More lenient for testing
            data.turbidityNTU = turbidity;
            displaySensorReading("🌊 Turbidity", data.turbidityNTU, "NTU");
            lastTurbidityRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid turbidity");
        }
    }

    // --- DHT ---
    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        float airTemp, airHumidity;
        
        if (USE_MOCK_DATA) {
            airTemp = 20.0 + (rand() % 150) / 10.0;
            airHumidity = 40.0 + (rand() % 600) / 10.0;
        } else {
            airTemp = read_dhtTemp();
            airHumidity = read_dhtHumidity();
        }
        
        if (!isnan(airTemp) && !isnan(airHumidity) && 
            airTemp > -40.0f && airTemp < 80.0f &&
            airHumidity >= 0.0f && airHumidity <= 100.0f) {
            data.airTemp = airTemp;
            data.airHumidity = airHumidity;
            displaySensorReading("🌡️ Air Temp", data.airTemp, "°C");
            displaySensorReading("💨 Air Humidity", data.airHumidity, "%");
            lastDHTRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid DHT");
        }
    }

    // --- Float Switch ---
    if (now - lastFloatRead >= FLOAT_READ_INTERVAL) {
        bool floatState = USE_MOCK_DATA ? (rand() % 2) : float_switch_active();
        if (floatState != lastFloatState) {
            data.floatTriggered = floatState;
            Serial.printf("💧 Float Switch: %s\n", floatState ? "TRIGGERED" : "NORMAL");
            lastFloatState = floatState;
            updated = true;
        }
        lastFloatRead = now;
    }
    
    return updated;
}