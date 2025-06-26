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



unsigned long last_waterTemp_read = 0;
unsigned long last_ph_read = 0;
unsigned long last_do_read = 0;
unsigned long last_turbidity_read = 0;
unsigned long last_DHT_read = 0;

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
        
        float temp = read_dhtTemp(); // from temp_sensor.h
        if (!isnan(temp)) {
            current.airTemp = temp;
        }
        #ifdef ENABLE_LOGGING
            Serial.print("Water Temp: "); Serial.println(current.waterTemp);
        #endif
        last_waterTemp_read = now;
    }

    if (now - last_ph_read >= PH_READ_INTERVAL) {

        float ph = read_ph();  // from ph_sensor.h
        if (!isnan(ph)) {
            current.pH = ph;
        }
        #ifdef ENABLE_LOGGING
           Serial.print("pH: "); Serial.println(current.pH);
        #endif
        last_ph_read = now;
    }

    if (now - last_do_read >= DO_READ_INTERVAL) {

        float doVoltage = readDOVoltage();  // from do_sensor.h
        float dissolveOxygenMgL = read_dissolveOxygen(doVoltage, current.waterTemp);  // from do_sensor.h

        if (!isnan(dissolveOxygenMgL)) {
            current.dissolvedOxygen = dissolveOxygenMgL;
        }

        #ifdef ENABLE_LOGGING
            Serial.print("Dissolved Oxygen (mg/L): "); Serial.println(current.dissolvedOxygen);
        #endif
        last_do_read = now;
    }

    if (now - last_turbidity_read >= TURBIDITY_READ_INTERVAL) {
        float turbudityNTU = read_turbidity();  // from turbidity_sensor.h

        if (!isnan(turbudityNTU)) {
            current.turbidityNTU = turbudityNTU;
        }

        #ifdef ENABLE_LOGGING
            Serial.print("Turbidity (NTU): "); Serial.println(current.turbidityNTU);
        #endif
        last_turbidity_read = now;
    }

    if (now - last_DHT_read >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();

        if (!isnan(airTemp) && !isnan(airHumidity)) {
            current.airTemp = airTemp;
            current.airHumidity = airHumidity;
        }

        #ifdef ENABLE_LOGGING
            Serial.print("Air Temp: "); Serial.println(current.airTemp);
            Serial.print("Humidity: "); Serial.println(current.airHumidity);
        #endif
        last_DHT_read = now;
    }

    // Float switch (no interval-based reading)
    current.floatTriggered = is_float_switch_triggered();
    if (current.floatTriggered ) {
        
        #ifdef ENABLE_LOGGING
            Serial.print("Water Temp: "); Serial.println(current.waterTemp);
        #endif
    }
    // Add rules logic
   // apply_rules(current);  // if you want auto-actuation
    yield();
    
}
