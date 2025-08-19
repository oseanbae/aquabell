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
    bool floatTriggered = 0; // 0 = false, 1 = true

    struct {
        bool fan;
        bool light;
        bool waterPump;
        bool valve;
    } relayStates;
};  

