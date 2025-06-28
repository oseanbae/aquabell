#include "DallasTemperature.h"
#include "OneWire.h"
#include "sensor_config.h"

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);

void temp_sensor_init() {
    sensor.begin();
}

float read_waterTemp() {
    float temp = sensor.getTempCByIndex(0);
    if (temp == DEVICE_DISCONNECTED_C) {
#ifdef ENABLE_LOGGING
        Serial.println("DS18B20 disconnected");
#endif
        return NAN;
    }
    return temp;
}
