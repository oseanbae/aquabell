#include <Arduino.h>
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


unsigned long last_waterTemp_read = 0;
unsigned long last_ph_read = 5000;
unsigned long last_do_read = 10000;
unsigned long last_turbidity_read = 15000;
unsigned long last_DHT_read = 20000;

SensorData current;
SensorBuffer sensorBuffer;

void setup() {
    Serial.begin(115200);
    delay(2000); // Let Serial and sensors stabilize
    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();
    Serial.println("System ready.");
}

void loop() {
    unsigned long now = millis();

    if (now - last_waterTemp_read >= DS18B20_READ_INTERVAL) {
        float temp = read_waterTemp();
        if (!isnan(temp)) current.waterTemp = temp;
        #ifdef ENABLE_LOGGING
            Serial.print("Water Temp: "); Serial.println(current.waterTemp);
        #endif
        last_waterTemp_read = now;
    }

    if (now - last_ph_read >= PH_READ_INTERVAL) {
        float ph = read_ph();
        if (!isnan(ph)) current.pH = ph;
        #ifdef ENABLE_LOGGING
            Serial.print("pH: "); Serial.println(current.pH);
        #endif
        last_ph_read = now;
    }

    if (now - last_do_read >= DO_READ_INTERVAL) {
        float doVoltage = readDOVoltage();
        float dissolveOxygenMgL = read_dissolveOxygen(doVoltage, current.waterTemp);
        if (!isnan(dissolveOxygenMgL)) current.dissolvedOxygen = dissolveOxygenMgL;
        #ifdef ENABLE_LOGGING
            Serial.print("Dissolved Oxygen: "); Serial.println(current.dissolvedOxygen);
        #endif
        last_do_read = now;
    }

    if (now - last_turbidity_read >= TURBIDITY_READ_INTERVAL) {
        float turbidityNTU = read_turbidity();
        if (!isnan(turbidityNTU)) current.turbidityNTU = turbidityNTU;
        #ifdef ENABLE_LOGGING
            Serial.print("Turbidity: "); Serial.println(current.turbidityNTU);
        #endif
        last_turbidity_read = now;
    }

    if (now - last_DHT_read >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();
        if (!isnan(airTemp)) current.airTemp = airTemp;
        if (!isnan(airHumidity)) current.airHumidity = airHumidity;
        #ifdef ENABLE_LOGGING
            Serial.print("Air Temp: "); Serial.println(current.airTemp);
            Serial.print("Humidity: "); Serial.println(current.airHumidity);
        #endif
        last_DHT_read = now;
    }

    // Float switch (continuous check)
    current.floatTriggered = is_float_switch_triggered();
    if (current.floatTriggered) {
        #ifdef ENABLE_LOGGING
            Serial.println("Float switch triggered!");
        #endif
    }

    // 🔁 Unified LCD update handler
    lcd_display_update(current);
    // Optionally enable control logic
    // apply_rules(current);

    yield(); // Keep the system responsive
}
