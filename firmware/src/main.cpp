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

#include "RTClib.h"

unsigned long last_waterTemp_read = 0;
unsigned long last_ph_read = 0;
unsigned long last_do_read = 0;
unsigned long last_turbidity_read = 0;
unsigned long last_DHT_read = 0;
SensorData current;


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
        current.waterTemp = read_waterTemp();  // from temp_sensor.h
        last_waterTemp_read = now;
    }

    if (now - last_ph_read >= PH_READ_INTERVAL) {
        current.pH = read_ph();  // from ph_sensor.h
        last_ph_read = now;
    }

    if (now - last_do_read >= DO_READ_INTERVAL) {
        current.dissolvedOxygen = read_dissolveOxygen(readDOVoltage(), current.waterTemp);  // from do_sensor.h
        last_do_read = now;
    }

    if (now - last_turbidity_read >= TURBIDITY_READ_INTERVAL) {
        current.turbidityNTU = read_turbidity();  // from turbidity_sensor.h
        last_turbidity_read = now;
    }

    if (now - last_DHT_read >= DHT_READ_INTERVAL) {
        current.airTemp = read_dhtTemp();
        current.airHumidity = read_dhtHumidity();
        last_DHT_read = now;
    }

    // Float switch (no interval-based reading)
    current.floatTriggered = is_float_switch_triggered();

    // Add rules logic
   // evaluate_rules(current);  // if you want auto-actuation

    // Logging
#ifdef ENABLE_LOGGING
    Serial.println("----- Sensor Readings -----");
    Serial.print("Air Temp: "); Serial.println(current.airTemp);
    Serial.print("Humidity: "); Serial.println(current.airHumidity);
    Serial.print("Water Temp: "); Serial.println(current.waterTemp);
    Serial.print("Turbidity (NTU): "); Serial.println(current.turbidityNTU);
    Serial.print("Dissolved Oxygen (mg/L): "); Serial.println(current.dissolvedOxygen);
    Serial.print("pH: "); Serial.println(current.pH);
    if (current.floatTriggered) Serial.println("⚠️ Float switch just triggered!");
    Serial.println("---------------------------");
#endif

    delay(1000);  // light delay to avoid starving CPU
}
