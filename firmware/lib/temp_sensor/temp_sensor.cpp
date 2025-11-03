#include "DallasTemperature.h"
#include "OneWire.h"
#include "config.h"
#include <Arduino.h>

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);

// Initialize the water temperature sensor
void temp_sensor_init() {
    sensor.begin();
}

// Read the water temperature in Celsius (raw, no clamping)
float read_waterTemp() {
    sensor.requestTemperatures(); // Force fresh reading
    float temp = sensor.getTempCByIndex(0);

    if (temp == DEVICE_DISCONNECTED_C) {
        Serial.println("Water temp sensor disconnected!");
        return NAN;
    }
    return temp;
}