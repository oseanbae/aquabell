#define BLYNK_TEMPLATE_ID "TMPL6tHPnKMmo"
#define BLYNK_TEMPLATE_NAME "AquaBell"
#define BLYNK_AUTH_TOKEN "RIgXfYc3OLJBvkH-vQA8w-lJjR7Qwi_1"

#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
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

// === CONSTANTS ===
#define USE_MOCK_DATA false
RTC_DS3231 rtc;
RealTimeData current;
BlynkTimer timer;

// === VIRTUAL PIN MAP ===
// V0 - Water Temp
// V1 - pH
// V2 - Dissolved Oxygen
// V3 - Turbidity NTU
// V4 - Air Temp
// V5 - Air Humidity
// V6 - Float Switch

// === FLAGS ===
struct SensorUpdateFlags {
    bool tempUpdated = false;
    bool phUpdated = false;
    bool doUpdated = false;
    bool turbidityUpdated = false;
    bool dhtUpdated = false;
    bool floatUpdated = false;
};

// === FORWARD DECLARATIONS ===
void initAllModules();
std::pair<RealTimeData, SensorUpdateFlags> readSensors(unsigned long now);
RealTimeData readMockSensors(unsigned long now);

// === BLYNK SEND HELPERS ===
void sendTempHumidity() {
    Blynk.virtualWrite(V0, current.waterTemp);
    Blynk.virtualWrite(V4, current.airTemp);
    Blynk.virtualWrite(V5, current.airHumidity);
}
void sendPH()          { Blynk.virtualWrite(V1, current.pH); }
void sendDO()          { Blynk.virtualWrite(V2, current.dissolvedOxygen); }
void sendTurbidity()   { Blynk.virtualWrite(V3, current.turbidityNTU); }
void sendFloatSwitch() { Blynk.virtualWrite(V6, current.floatTriggered); }

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

    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
    Serial.println("✅ Blynk connected. System ready.");
}

// === LOOP ===
void loop() {
    timer.run();
    Blynk.run();
    

    unsigned long nowMillis = millis();

    DateTime now;
    if (rtc.begin()) {
        now = rtc.now();
    } else {
        Serial.println("⚠️ RTC unavailable — skipping loop.");
        return;
    }

    if (USE_MOCK_DATA) {
        current = readMockSensors(nowMillis);
        sendTempHumidity();
        sendPH();
        sendDO();
        sendTurbidity();
        sendFloatSwitch();
    } else {
        auto [newData, updated] = readSensors(nowMillis);
        current = newData;

        if (updated.tempUpdated || updated.dhtUpdated) sendTempHumidity();
        if (updated.phUpdated)                         sendPH();
        if (updated.doUpdated)                         sendDO();
        if (updated.turbidityUpdated)                  sendTurbidity();
        if (updated.floatUpdated)                      sendFloatSwitch();
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
std::pair<RealTimeData, SensorUpdateFlags> readSensors(unsigned long now) {
    static unsigned long lastTempRead = 0, lastPHRead = 0, lastDORead = 0;
    static unsigned long lastTurbidityRead = 0, lastDHTRead = 0, lastFloatRead = 0;

    RealTimeData data = current;
    SensorUpdateFlags flags;

    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        float temp = read_waterTemp();
        if (!isnan(temp)) {
            data.waterTemp = temp;
            flags.tempUpdated = true;
        }
        lastTempRead = now;
    }

    if (now - lastPHRead >= PH_READ_INTERVAL) {
        float ph = read_ph();
        if (!isnan(ph)) {
            data.pH = constrain(ph, 0.0, 14.0);
            flags.phUpdated = true;
        }
        lastPHRead = now;
    }

    if (now - lastDORead >= DO_READ_INTERVAL) {
        float voltage = readDOVoltage();
        float doVal = read_dissolveOxygen(voltage, data.waterTemp);
        if (!isnan(doVal)) {
            data.dissolvedOxygen = max(doVal, 0.0f);
            flags.doUpdated = true;
        }
        lastDORead = now;
    }

    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        float ntu = read_turbidity();
        if (!isnan(ntu)) {
            data.turbidityNTU = max(ntu, 0.0f);
            flags.turbidityUpdated = true;
        }
        lastTurbidityRead = now;
    }

    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();
        if (!isnan(airTemp)) {
            data.airTemp = airTemp;
            flags.dhtUpdated = true;
        }
        if (!isnan(airHumidity)) {
            data.airHumidity = airHumidity;
            flags.dhtUpdated = true;
        }
        lastDHTRead = now;
    }

    if (now - lastFloatRead >= FLOAT_READ_INTERVAL) {
        lastFloatRead = now;
        static bool lastFloatState = false;
        bool floatState = float_switch_active();
        if (floatState != lastFloatState) {
        data.floatTriggered = floatState;
        flags.floatUpdated = true;
        lastFloatState = floatState;
        }
    }

    return { data, flags };
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
