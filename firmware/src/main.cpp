#include <Arduino.h>
#include <WiFi.h>
#include <RTClib.h>

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

// === CONSTANTS ===
#define USE_MOCK_DATA false

RTC_DS3231 rtc;
RealTimeData current;

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

    // Initialize RTC with timeout
    int rtcAttempts = 0;
    while (!rtc.begin() && rtcAttempts < 10) {
        Serial.println("❌ RTC not found. Retrying...");
        delay(3000);
        rtcAttempts++;
    }
    
    if (rtcAttempts >= 10) {
        Serial.println("❌ RTC initialization failed. Continuing without RTC.");
    } else {
        Serial.println("✅ RTC initialized successfully.");
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
            Serial.println(" Attempting WiFi reconnection...");
            WiFi.reconnect();
            lastWiFiAttempt = nowMillis;
        }
    }

    // Read sensors
    bool updated = readSensors(nowMillis, current);
    
    // ← ENABLE RULE ENGINE
    if (updated || is_float_switch_triggered()) {
        apply_rules(current, rtc.now());
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
        if (nowMillis - lastLiveUpdate >= liveInterval) {
            pushToFirestoreLive(current);
            logBuffer[logIndex++] = current;
            lastLiveUpdate = nowMillis;
        }

            if ((nowMillis - lastBatchLog >= batchInterval) || (logIndex >= batchSize)) {
        pushBatchLogToFirestore(logBuffer, logIndex, rtc.now());
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
            displaySensorReading("Water Temp", data.waterTemp, "°C");
            lastTempRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid water temperature reading");
        }
    }

    // --- pH ---
    if (now - lastPHRead >= PH_READ_INTERVAL) {
        float ph = USE_MOCK_DATA ? 
            6.5 + (rand() % 50) / 10.0 : read_ph();
        
        if (!isnan(ph) && ph > 0.0f && ph < 14.0f) {
            data.pH = ph;
            displaySensorReading("pH", data.pH, "");
            lastPHRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid pH reading");
        }
    }

    // --- Dissolved Oxygen ---
    if (now - lastDORead >= DO_READ_INTERVAL) {
        float doValue = USE_MOCK_DATA ? 
            5.0 + (rand() % 40) / 10.0 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
        
        if (!isnan(doValue) && doValue >= 0.0f && doValue < 20.0f) {
            data.dissolvedOxygen = doValue;
            displaySensorReading("Dissolved Oxygen", data.dissolvedOxygen, "mg/L");
            lastDORead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid DO reading");
        }
    }

    // --- Turbidity ---
    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        float turbidity = USE_MOCK_DATA ? 
            (rand() % 1000) / 10.0 : read_turbidity();
        
        if (!isnan(turbidity) && turbidity >= 0.0f && turbidity < 2000.0f) {
            data.turbidityNTU = turbidity;
            displaySensorReading("Turbidity", data.turbidityNTU, "NTU");
            lastTurbidityRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid turbidity reading");
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
            displaySensorReading("Air Temp", data.airTemp, "°C");
            displaySensorReading("Air Humidity", data.airHumidity, "%");
            lastDHTRead = now;
            updated = true;
        } else {
            Serial.println("⚠️ Invalid DHT readings");
        }
    }

    // --- Float Switch ---
    if (now - lastFloatRead >= FLOAT_READ_INTERVAL) {
        bool floatState = USE_MOCK_DATA ? (rand() % 2) : float_switch_active();
        if (floatState != lastFloatState) {
            data.floatTriggered = floatState;
            Serial.printf("[SENSOR] Float Switch = %s\n", floatState ? "TRIGGERED" : "NORMAL");
            lastFloatState = floatState;
            updated = true;
        }
        lastFloatRead = now;
    }
    
    return updated;
}
