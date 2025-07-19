#include <Arduino.h>
#include "config.h"

// Sensor libraries
#include "temp_sensor.h"
#include "ph_sensor.h"
#include "do_sensor.h"
#include "turbidity_sensor.h"
#include "dht_sensor.h"
#include "float_switch.h"
#include "sensor_data.h"
#include "rule_engine.h"
#include "hivemq.h"
#include "mqtt_util.h"
#include "mqtt_topics.h"
// Display libraries


#include "lcd_display.h"
#define USE_MOCK_DATA false
RealTimeData current;     // Current sensor readings
RealTimeData lastSent;    // Last sent data snapshot


void setup() {
    Serial.begin(115200);
    delay(2000);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();

    lcd_init();
    setup_wifi();
    setup_mqtt();

    Serial.println("System ready.");
}

void loop() {
    mqtt_loop();

    unsigned long now = millis();
    static unsigned long lastSentTime = 0;
    current = USE_MOCK_DATA ? readMockSensors(now) : readSensors(now);

    // Optional: Uncomment if you want rules & LCD
    // lcd_display_update(current);
    // apply_rules(current);

    bool changed = true; // Always publish in mock mode or add delta checks here

    if (changed || (now - lastSentTime >= 10000)) {
        String payload = sensorDataToJson(current);
        Serial.println(payload);
        mqtt_publish("aquabell/sensors", payload);

        lastSent = current;
        lastSentTime = now;
    }

    yield(); // Feed watchdog
}

RealTimeData readSensors(unsigned long now) {
    static unsigned long lastTempRead = 0;
    static unsigned long lastPHRead = 0;
    static unsigned long lastDORead = 0;
    static unsigned long lastTurbidityRead = 0;
    static unsigned long lastDHTRead = 0;
    static unsigned long lastFloatDebounce = 0;

    RealTimeData data = current; // Start from last known data

    // Water Temperature
    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        float temp = read_waterTemp();
        if (!isnan(temp)) data.waterTemp = temp;
        lastTempRead = now;
    }

    // pH
    if (now - lastPHRead >= PH_READ_INTERVAL) {
        float ph = read_ph();
        if (!isnan(ph)) data.pH = constrain(ph, 0.0, 14.0);
        lastPHRead = now;
    }

    // Dissolved Oxygen
    if (now - lastDORead >= DO_READ_INTERVAL) {
        float voltage = readDOVoltage();
        float doVal = read_dissolveOxygen(voltage, data.waterTemp);
        if (!isnan(doVal)) data.dissolvedOxygen = max(doVal, 0.0f);
        lastDORead = now;
    }

    // Turbidity
    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        float ntu = read_turbidity();
        if (!isnan(ntu)) data.turbidityNTU = max(ntu, 0.0f);
        lastTurbidityRead = now;
    }

    // Air Temperature & Humidity (DHT)
    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();
        if (!isnan(airTemp)) data.airTemp = airTemp;
        if (!isnan(airHumidity)) data.airHumidity = airHumidity;
        lastDHTRead = now;
    }

    // Float Switch
    if (now - lastFloatDebounce >= 100) {
        bool floatState = float_switch_active();
        data.floatTriggered = floatState;
        digitalWrite(LED_PIN, floatState ? HIGH : LOW);
        lastFloatDebounce = now;
    }

    return data;
}
RealTimeData readMockSensors(unsigned long now) {
    RealTimeData mockData;

    mockData.waterTemp = 26.5 + random(-10, 10) * 0.1;         // 25.5 - 27.5
    mockData.pH = 7.0 + random(-10, 10) * 0.01;                 // 6.9 - 7.1
    mockData.dissolvedOxygen = 5.5 + random(-20, 20) * 0.05;    // 4.5 - 6.5
    mockData.turbidityNTU = 50 + random(-100, 100) * 0.1;       // 40 - 60
    mockData.airTemp = 28.0 + random(-10, 10) * 0.1;            // 27 - 29
    mockData.airHumidity = 60.0 + random(-100, 100) * 0.1;      // 50 - 70
    mockData.floatTriggered = random(0, 2);                     // 0 or 1

    return mockData;
}
