#pragma once
#include <Arduino.h>
#include <math.h>

struct SensorData {
    float waterTemp = NAN;         // Water temperature in Celsius
    float pH = NAN;                // pH level
    float dissolvedOxygen = NAN;   // Dissolved oxygen level
    float turbidityNTU = NAN;      // Turbidity in NTU
    float airTemp = NAN;           // Air temperature in Celsius
    float airHumidity = NAN;       // Air humidity in percentage
    bool floatTriggered = false;
};
