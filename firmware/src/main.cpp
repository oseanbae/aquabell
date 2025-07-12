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

    static unsigned long lastTempRead = 0;
    static unsigned long lastPHRead = 0;
    static unsigned long lastDORead = 0;
    static unsigned long lastTurbidityRead = 0;
    static unsigned long lastDHTRead = 0;
    static unsigned long lastFloatDebounce = 0;
    static unsigned long lastSentTime = 0;

    // === Sensor Intervals ===

    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        float temp = read_waterTemp();
        if (!isnan(temp)) current.waterTemp = temp;
        lastTempRead = now;
    }

    if (now - lastPHRead >= PH_READ_INTERVAL) {
        float ph = read_ph();
        if (!isnan(ph)) current.pH = constrain(ph, 0.0, 14.0);
        lastPHRead = now;
    }

    if (now - lastDORead >= DO_READ_INTERVAL) {
        float voltage = readDOVoltage();
        float doVal = read_dissolveOxygen(voltage, current.waterTemp);
        if (!isnan(doVal)) current.dissolvedOxygen = max(doVal, 0.0f);
        lastDORead = now;
    }

    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        float ntu = read_turbidity();
        if (!isnan(ntu)) current.turbidityNTU = max(ntu, 0.0f);
        lastTurbidityRead = now;
    }

    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();
        if (!isnan(airTemp)) current.airTemp = airTemp;
        if (!isnan(airHumidity)) current.airHumidity = airHumidity;
        lastDHTRead = now;
    }

    if (now - lastFloatDebounce >= 100) {
        bool floatState = float_switch_active();
        current.floatTriggered = floatState;
        digitalWrite(LED_PIN, floatState ? HIGH : LOW);
        lastFloatDebounce = now;
    }

    // === LCD & Rules ===
    lcd_display_update(current);
    apply_rules(current);

    // === Change Detection or Heartbeat ===
    bool changed = false;

    changed |= abs(current.waterTemp - lastSent.waterTemp) > 0.1;
    changed |= abs(current.pH - lastSent.pH) > 0.05;
    changed |= abs(current.dissolvedOxygen - lastSent.dissolvedOxygen) > 0.1;
    changed |= abs(current.turbidityNTU - lastSent.turbidityNTU) > 1.0;
    changed |= abs(current.airTemp - lastSent.airTemp) > 0.2;
    changed |= abs(current.airHumidity - lastSent.airHumidity) > 1.0;
    changed |= current.floatTriggered != lastSent.floatTriggered;

    if (changed || (now - lastSentTime >= 10000)) {
        String payload = sensorDataToJson(current);
        Serial.println(payload);
        mqtt_publish("aquabell/sensors", payload);

        lastSent = current;
        lastSentTime = now;
    }

    yield(); // Feed watchdog
}
