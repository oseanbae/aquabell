#pragma once
#include <Arduino.h>
#include <RTClib.h>  // 👈 Add this to resolve DateTime
#include <math.h> // For NAN


struct SensorData {
    DateTime timestamp;
    float waterTemp = NAN;         // Water temperature in Celsius
    float pH = NAN;                // pH level
    float dissolvedOxygen = NAN;   // Dissolved oxygen level
    float turbidityNTU = NAN;      // Turbidity in NTU
    float airTemp = NAN;           // Air temperature in Celsius
    float airHumidity = NAN;       // Air humidity in percentage
    bool floatTriggered = false;
};

struct SensorBuffer {
    float waterTempSum = 0;
    float waterTempCount = 0;

    float pHSum = 0;
    float pHCount = 0;

    float doSum;
    float doCount;

    float turbiditySum = 0;
    float turbidityCount = 0;

    float airTempSum = 0;
    float airTempCount = 0;

    float airHumiditySum = 0;
    float airHumidityCount = 0;

};