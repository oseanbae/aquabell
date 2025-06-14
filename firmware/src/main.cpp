#include <Arduino.h>
#include "config.h"
#include "dht_sensor.h"
#include "temp_sensor.h"
#include "float_switch.h"   
#include "turbidity_sensor.h"
#include "ph_sensor.h"

unsigned long lastSensorRead = 0; // Variable to store the last time sensors were read
const unsigned long SENSOR_INTERVAL = 3000; // Interval to read sensors (3 seconds)

void setup() {
    Serial.begin(115200); // Initialize serial communication at 115200 baud rate
    dht_sensor_init(); // Initialize the DHT sensor
    temp_sensor_init(); // Initialize the temperature sensor (DS18B20)
    float_switch_init(); // Initialize the float switch (if used)
    turbidity_sensor_init(); // Initialize the turbidity sensor
    ph_sensor_init(); // Initialize the pH sensor
    Serial.println("ESP32 Water Quality Monitor Initialized.");
    Serial.println("Monitoring water quality parameters...");
}   
void loop() {
     float_switch_read();

    // ⏱️ Sensors are read only every 3 seconds
    if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
        lastSensorRead = millis();
        dht_sensor_read();
        temp_sensor_read();
        turbidity_sensor_read(); 
        ph_sensor_read(); 
    }
}
