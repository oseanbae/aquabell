#include "dht_sensor.h"
#include "DHT.h"
#include "sensor_config.h"
#include <Arduino.h>

// DHT sensor configuration
DHT dht(DHT_PIN, DHT_TYPE);

// Initialize the DHT sensor
void dht_sensor_init() {
    dht.begin();
}

// Clamp function to ensure values are within a specified range
// This function ensures that the value is not less than min_val and not greater than max_val
static float clamp(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

// Read and clamp temperature in Celsius
float read_dhtTemp() {
    float temp = dht.readTemperature();
    if (isnan(temp)) {
        Serial.println("DHT temperature read failed.");
        return -999.0f;
    }
    return clamp(temp, -40.0f, 80.0f); // DHT11/22 safe range
}

// Read and clamp humidity in %
float read_dhtHumidity() {
    float hum = dht.readHumidity();
    if (isnan(hum)) {
        Serial.println("DHT humidity read failed.");
        return -1.0f;
    }
    return clamp(hum, 0.0f, 100.0f);
}