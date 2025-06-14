#include <Arduino.h>
#include "config.h"
#include "dht_sensor.h"
#include "temp_sensor.h"
#include "float_switch.h"   

unsigned long lastSensorRead = 0; // Variable to store the last time sensors were read
const unsigned long SENSOR_INTERVAL = 3000; // Interval to read sensors (3 seconds)

void setup() {
    Serial.begin(115200); // Initialize serial communication at 115200 baud rate
    dht_sensor_init(); // Initialize the DHT sensor
    temp_sensor_init(); // Initialize the temperature sensor (DS18B20)
    float_switch_init(); // Initialize the float switch (if used)
   
}   

void loop() {
     float_switch_read();

    // ⏱️ Sensors are read only every 3 seconds
    if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
        lastSensorRead = millis();
        dht_sensor_read();
        temp_sensor_read();
    }
}
