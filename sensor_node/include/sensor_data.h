#pragma once
#include <Arduino.h>
#include <math.h>


struct RealTimeData {
    float waterTemp = NAN;         // Water temperature in Celsius
    float pH = NAN;                // pH level
    float dissolvedOxygen = NAN;   // Dissolved oxygen level
    float turbidityNTU = NAN;      // Turbidity in NTU
    float airTemp = NAN;           // Air temperature in Celsius
    float airHumidity = NAN;       // Air humidity in percentage
    bool floatTriggered = false;
};

struct BatchData {
    float waterTempSum = 0;
    uint16_t  waterTempCount = 0;

    float pHSum = 0;
    uint16_t pHCount = 0;

    float doSum;
    uint16_t doCount;

    float turbiditySum = 0;
    uint16_t  turbidityCount = 0;

    float airTempSum = 0;
    uint16_t  airTempCount = 0;

    float airHumiditySum = 0;
    uint16_t  airHumidityCount = 0;

};