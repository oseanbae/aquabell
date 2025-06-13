#include <Arduino.h>
#include "config.h"
#include "dht_sensor.h"
#include "temp_sensor.h"
void setup() {
    Serial.begin(115200); // Initialize serial communication at 115200 baud rate
    dht_sensor_init(); // Initialize the DHT sensor
    temp_sensor_init(); // Initialize the temperature sensor (DS18B20)
    Serial.println("DHT and DS18B20 sensors initialized.");
}   

void loop() {
    dht_sensor_read(); // Read and print temperature and humidity
    delay(2000); // Wait for 2 seconds before the next reading
    temp_sensor_read(); // Read and print temperature from DS18B20 sensor
    delay(2000); // Wait for 2 seconds before the next reading
}
