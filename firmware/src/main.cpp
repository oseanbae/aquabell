#include <Arduino.h>
#include <Wifi.h>
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
#define USE_MOCK_DATA true
//C:\Users\<your-user>\AppData\Local\Android\Sdk
RTC_DS3231 rtc;
RealTimeData current;


void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected.");
}

// === FORWARD DECLARATIONS ===
void initAllModules();
RealTimeData readSensors(unsigned long now);
RealTimeData readMockSensors(unsigned long now);

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

    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.println("⚠️ RTC reset to compile time.");
    }

    initAllModules();
    connectWiFi();

    // >>> Sign-in to Firebase on boot
    firebaseSignIn();
}

// === LOOP ===
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }

    unsigned long nowMillis = millis();

    DateTime now;

    if (rtc.begin()) {
        now = rtc.now();
    } else {
        Serial.println("⚠️ RTC unavailable — skipping loop.");
        return;
    }
    static unsigned long lastLiveUpdate = 0;
    static unsigned long lastBatchLog = 0;
    const unsigned long liveInterval = 10000;
    const unsigned long batchInterval = 600000;
    const int batchSize = 60;
    static RealTimeData logBuffer[batchSize];
    static int logIndex = 0;

    // Read new sensor values (or mock if enabled)
    current = USE_MOCK_DATA ? readMockSensors(nowMillis) : readSensors(nowMillis);

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

    apply_rules(current, now);
    lcd_display_update(current); 

    yield();  // Feed watchdog
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
}

// === SENSOR READ FUNCTION ===
RealTimeData readSensors(unsigned long now) {
    static unsigned long lastTempRead = 0, lastPHRead = 0, lastDORead = 0;
    static unsigned long lastTurbidityRead = 0, lastDHTRead = 0, lastFloatRead = 0;

    RealTimeData data = current;


    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        float temp = read_waterTemp();
        if (!isnan(temp)) {
            data.waterTemp = temp;

        }
        lastTempRead = now;
    }

    if (now - lastPHRead >= PH_READ_INTERVAL) {
        float ph = read_ph();
        if (!isnan(ph)) {
            data.pH = constrain(ph, 0.0, 14.0);
        }
        lastPHRead = now;
    }

    if (now - lastDORead >= DO_READ_INTERVAL) {
        float voltage = readDOVoltage();
        float doVal = read_dissolveOxygen(voltage, data.waterTemp);
        if (!isnan(doVal)) {
            data.dissolvedOxygen = max(doVal, 0.0f);
        }
        lastDORead = now;
    }

    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        float ntu = read_turbidity();
        if (!isnan(ntu)) {
            data.turbidityNTU = max(ntu, 0.0f);
        }
        lastTurbidityRead = now;
    }

    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();
        if (!isnan(airTemp)) {
            data.airTemp = airTemp;
        }
        if (!isnan(airHumidity)) {
            data.airHumidity = airHumidity;
        }
        lastDHTRead = now;
    }

    if (now - lastFloatRead >= FLOAT_READ_INTERVAL) {
        lastFloatRead = now;
        static bool lastFloatState = false;
        bool floatState = float_switch_active();
        if (floatState != lastFloatState) {
        data.floatTriggered = floatState;
        lastFloatState = floatState;
        }
    }

    return data;
}

// === MOCK SENSOR DATA ===
RealTimeData readMockSensors(unsigned long now) {
    RealTimeData d;
    d.waterTemp = 26.5 + random(-10, 10) * 0.1;
    d.pH = 7.0 + random(-10, 10) * 0.01;
    d.dissolvedOxygen = 5.5 + random(-20, 20) * 0.05;
    d.turbidityNTU = 50 + random(-100, 100) * 0.1;
    d.airTemp = 28.0 + random(-10, 10) * 0.1;
    d.airHumidity = 60.0 + random(-100, 100) * 0.1;
    d.floatTriggered = random(0, 2);
    return d;
}
