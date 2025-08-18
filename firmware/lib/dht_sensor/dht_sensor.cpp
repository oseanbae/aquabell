#include "dht_sensor.h"
#include "DHT.h"
#include "config.h"
#include <Arduino.h>

// DHT sensor configuration
DHT dht(DHT_PIN, DHT_TYPE);

// Initialize the DHT sensor
void dht_sensor_init() {
    dht.begin();
}

// Read temperature in Celsius (raw, only clamp to physical safe range)
float read_dhtTemp() {
    float temp = dht.readTemperature();
    if (isnan(temp)) {
        Serial.println("DHT temperature read failed.");
        return NAN;
    }
    // DHT spec range check: -40 to 80 °C
    if (temp < -40.0f) temp = -40.0f;
    if (temp > 80.0f)  temp = 80.0f;
    return temp;
}

// Read humidity in %
float read_dhtHumidity() {
    float hum = dht.readHumidity();
    if (isnan(hum)) {
        Serial.println("DHT humidity read failed.");
        return NAN;
    }
    // Clamp to physical 0–100 %
    if (hum < 0.0f)   hum = 0.0f;
    if (hum > 100.0f) hum = 100.0f;
    return hum;
}
