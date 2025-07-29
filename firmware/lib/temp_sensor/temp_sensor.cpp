#include "DallasTemperature.h"
#include "OneWire.h"
#include "config.h"

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor(&oneWire);

// Temperature limits for tilapia
const float TEMP_WARN_LOW = 16.0f;   // Warning threshold (tilapia min)
const float TEMP_WARN_HIGH = 30.0f;  // Warning threshold (tilapia max)

// Initialize the water temperature sensor
void temp_sensor_init() {
    sensor.begin();
}

// Clamp function to ensure values are within a specified range
static float clamp(float val, float minVal, float maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

// Read the water temperature in Celsius
float read_waterTemp() {
    sensor.requestTemperatures(); // Force fresh reading
    float temp = sensor.getTempCByIndex(0);
    return clamp(temp, TEMP_WARN_LOW, TEMP_WARN_HIGH);
}
