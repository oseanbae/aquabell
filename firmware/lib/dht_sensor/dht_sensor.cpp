#include "dht_sensor.h"
#include "DHT.h"
#include "config.h"

DHT dht(DHT_PIN, DHT_TYPE);

void dht_sensor_init() {
    dht.begin();
}

float dht_read_temperature() {
    float t = dht.readTemperature();
    if (isnan(t)) {
#ifdef ENABLE_LOGGING
        Serial.println("Failed to read DHT temperature");
#endif
        return NAN;
    }
    return t;
}

float dht_read_humidity() {
    float h = dht.readHumidity();
    if (isnan(h)) {
#ifdef ENABLE_LOGGING
        Serial.println("Failed to read DHT humidity");  
#endif
        return NAN;
    }
    return h;
}
