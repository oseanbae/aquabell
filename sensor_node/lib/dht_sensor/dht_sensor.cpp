#include "dht_sensor.h"
#include "DHT.h"
#include "sensor_config.h"

DHT dht(DHT_PIN, DHT_TYPE);

void dht_sensor_init() {
    dht.begin();
}

float read_dhtTemp() {
    float t = dht.readTemperature();
    if (isnan(t)) {
#ifdef ENABLE_LOGGING
        Serial.println("Failed to read DHT temperature");
#endif
        return NAN;
    }
    return t;
}

float read_dhtHumidity() {
    float h = dht.readHumidity();
    if (isnan(h)) {
#ifdef ENABLE_LOGGING
        Serial.println("Failed to read DHT humidity");  
#endif
        return NAN;
    }
    return h;
}
