#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

struct RealTimeData {
    float waterTemp = NAN;
    float pH = NAN;
    float dissolvedOxygen = NAN;
    float turbidityNTU = NAN;
    float airTemp = NAN;
    float airHumidity = NAN;
    uint8_t floatTriggered = 0;
    uint8_t isBatch = 0;
};