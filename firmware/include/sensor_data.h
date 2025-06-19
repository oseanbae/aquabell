#pragma once
#include <Arduino.h>
#include <math.h> // For NAN

struct SensorData {
    float airTemp = NAN;
    float airHumidity = NAN;
    float waterTemp = NAN;
    float turbidityNTU = NAN;
    float pH = NAN;
    bool floatTriggered = false;
    const char* turbidityClass = "Unknown";
};
