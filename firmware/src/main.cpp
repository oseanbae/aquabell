#include <Arduino.h>
#include "config.h"
#include "dht_sensor.h"
#include "temp_sensor.h"
#include "turbidity_sensor.h"


void setup() {
  Serial.begin(115200);
  delay(2000); // Optional warm-up for sensors

  // Initialize sensors
  dht_sensor_init();
  temp_sensor_init();
  turbidity_sensor_init();

  Serial.println("ESP32 Water Quality Monitor Initialized.");
  Serial.println("Monitoring water quality based on custom aquaculture standards.");
  Serial.println("-----------------------------------------------------");
}

void loop() {
  dht_sensor_read();
  temp_sensor_read();
  turbidity_sensor_read();
  delay(5000); // Delay between readings, adjust as needed
}