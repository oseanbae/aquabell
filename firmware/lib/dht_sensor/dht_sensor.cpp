#include "dht_sensor.h"
#include "DHT.h"
#include "config.h"
#include <Arduino.h>

// DHT sensor configuration
DHT dht(DHT_PIN, DHT_TYPE);

// Internal smoothed values
static float smoothedTemp = NAN;
static float smoothedHumidity = NAN;

// Tuning constant: 0.1 = heavy smoothing (slow response), 0.3 = faster
const float DHT_SMOOTH_ALPHA = 0.25f;

// Internal smoothing helper
float smoothValue(float current, float previous, float alpha) {
    if (isnan(current)) return previous; // ignore invalid samples
    if (isnan(previous)) return current; // first valid sample
    return (alpha * current) + ((1.0f - alpha) * previous);
}

// Initialize the DHT sensor
void dht_sensor_init() {
    dht.begin();
    smoothedTemp = NAN;
    smoothedHumidity = NAN;
}

// Read temperature in Celsius (smoothed)
float read_dhtTemp() {
    float temp = dht.readTemperature();
    if (isnan(temp)) {
        return smoothedTemp; // return last valid smoothed value
    }

    // Clamp to physical range
    temp = constrain(temp, -40.0f, 80.0f);

    // Apply smoothing filter
    smoothedTemp = smoothValue(temp, smoothedTemp, DHT_SMOOTH_ALPHA);
    return smoothedTemp;
}

// Read humidity in % (smoothed)
float read_dhtHumidity() {
    float hum = dht.readHumidity();
    if (isnan(hum)) {
        return smoothedHumidity; // return last valid smoothed value
    }

    // Clamp to physical range
    hum = constrain(hum, 0.0f, 100.0f);

    // Apply smoothing filter
    smoothedHumidity = smoothValue(hum, smoothedHumidity, DHT_SMOOTH_ALPHA);
    return smoothedHumidity;
}
