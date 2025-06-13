#include <Arduino.h>
#include "config.h"
#include "DallasTemperature.h"
#include "OneWire.h"

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire); //DS18B20 temperature sensor

void temp_sensor_init() {
    sensor.begin(); // Initialize the temperature sensor
}

void temp_sensor_read() {
    sensor.requestTemperatures(); // Request temperature readings
    float temperature = sensor.getTempCByIndex(0); // Get temperature in Celsius from the first sensor

    if (temperature == DEVICE_DISCONNECTED_C) {
        Serial.println("Error: Could not read temperature from DS18B20 sensor!");
    } else {
        Serial.print("DS18B20 Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");
    }
}