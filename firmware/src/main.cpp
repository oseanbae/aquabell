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

unsigned long lastSensorRead = 0;

#ifdef ENABLE_LOGGING
    Serial.println("----- Sensor Readings -----");
    Serial.print("Air Temp: "); Serial.println(current.airTemp);
    Serial.print("Humidity: "); Serial.println(current.airHumidity);
    Serial.print("Water Temp: "); Serial.println(current.waterTemp);
    Serial.print("Turbidity (NTU): "); Serial.println(current.turbidityNTU);
    Serial.print("Turbidity Quality: "); Serial.println(current.turbidityClass);
    Serial.print("Dissolved Oxygen (mg/L): "); Serial.println(current.dissolvedOxygen);
    
    Serial.print("pH: "); Serial.println(current.pH);
    if (current.floatTriggered) Serial.println("⚠️ Float switch just triggered!");
    Serial.println("---------------------------");
#endif

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
    
}
