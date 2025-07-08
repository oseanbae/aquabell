#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

struct __attribute__((packed)) RealTimeData {
    float waterTemp = NAN;
    float pH = NAN;
    float dissolvedOxygen = NAN;
    float turbidityNTU = NAN;
    float airTemp = NAN;
    float airHumidity = NAN;
    uint8_t floatTriggered = 0; // 0 = false, 1 = true
    uint8_t isBatch = 0;        // 0 = false, 1 = true
};

struct BatchData {
    float waterTempSum = 0;
    uint16_t  waterTempCount = 0;

    float pHSum = 0;
    uint16_t pHCount = 0;

    float doSum = 0;
    uint16_t doCount = 0;

    float turbiditySum = 0;
    uint16_t  turbidityCount = 0;

    float airTempSum = 0;
    uint16_t  airTempCount = 0;

    float airHumiditySum = 0;
    uint16_t  airHumidityCount = 0;
};
