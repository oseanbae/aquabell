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
#include "lcd_display.h"
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

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    while (!rtc.begin()) {
        Serial.println("❌ RTC not found. Retrying...");
        delay(3000);
    }

    // if (rtc.lostPower()) {
    //     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    //     Serial.printl    n("⚠️ RTC reset to compile time.");
    // }

    connectWiFi();

    //>>> Sign-in to Firebase on boot
    firebaseSignIn();
    initAllModules();
}

// === LOOP ===
unsigned long lastRuleCheck = 0;

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    unsigned long nowMillis = millis(); 

    static unsigned long lastLiveUpdate = 0;
    static unsigned long lastBatchLog = 0;
    const unsigned long liveInterval = 10000;
    const unsigned long batchInterval = 600000;
    const int batchSize = 60;
    static RealTimeData logBuffer[batchSize];
    static int logIndex = 0;

    bool updated = readSensors(nowMillis, current);
    // if (updated || is_float_switch_triggered()) {
    //     apply_rules(current, rtc.now());
    // }

    if (nowMillis - lastLiveUpdate >= liveInterval) {
        pushToFirestoreLive(current);
        logBuffer[logIndex++] = current;
        lastLiveUpdate = nowMillis;
    }


    if ((nowMillis - lastBatchLog >= batchInterval) || (logIndex >= batchSize)) {
        pushBatchLogToFirestore(logBuffer, logIndex);
        logIndex = 0;
        lastBatchLog = nowMillis;
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
    lcd_init();
    Serial.println("✅ All modules initialized successfully.");
}

// === SENSOR READ FUNCTION ===
bool readSensors(unsigned long now, RealTimeData &data) {
    static unsigned long lastTempRead = 0, lastPHRead = 0, lastDORead = 0;
    static unsigned long lastTurbidityRead = 0, lastDHTRead = 0, lastFloatRead = 0;
    static bool lastFloatState = false;

    bool updated = false;
    data = current;

    // --- Water Temp ---
    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        data.waterTemp = USE_MOCK_DATA ? 
            25.0 + (rand() % 100) / 10.0 : read_waterTemp();
        displaySensorReading("Water Temp", data.waterTemp, "°C");
        lastTempRead = now;
        updated = true;
    }

    // --- pH ---
    if (now - lastPHRead >= PH_READ_INTERVAL) {
        data.pH = USE_MOCK_DATA ? 
            6.5 + (rand() % 50) / 10.0 : read_ph();
        displaySensorReading("pH", data.pH, "");
        lastPHRead = now;
        updated = true;
    }

    // --- Dissolved Oxygen ---
    if (now - lastDORead >= DO_READ_INTERVAL) {
        data.dissolvedOxygen = USE_MOCK_DATA ? 
            5.0 + (rand() % 40) / 10.0 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
        displaySensorReading("Dissolved Oxygen", data.dissolvedOxygen, "mg/L");
        lastDORead = now;
        updated = true;
    }

    // --- Turbidity ---
    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        data.turbidityNTU = USE_MOCK_DATA ? 
            (rand() % 1000) / 10.0 : read_turbidity();
        displaySensorReading("Turbidity", data.turbidityNTU, "NTU");
        lastTurbidityRead = now;
        updated = true;
    }

    // --- DHT ---
    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        if (USE_MOCK_DATA) {
            data.airTemp = 20.0 + (rand() % 150) / 10.0;
            data.airHumidity = 40.0 + (rand() % 600) / 10.0;
        } else {
            data.airTemp = read_dhtTemp();
            data.airHumidity = read_dhtHumidity();
        }
        displaySensorReading("Air Temp", data.airTemp, "°C");
        displaySensorReading("Air Humidity", data.airHumidity, "%");
        lastDHTRead = now;
        updated = true;
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
