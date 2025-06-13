#include <Arduino.h>
#include "config.h"
#include "dht_sensor.h"
#include "DHT.h"

// Initialize the DHT sensor
 DHT dht(DHT_PIN, DHT_TYPE);

void dht_sensor_init() {
    dht.begin();
}

void dht_sensor_read() {
    // Read temperature and humidity
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Check if the readings are valid
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    // Print the results
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C");
    
    Serial.print(" | Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
}