#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// Centralized actuator state management

struct ActuatorState {
    // Current physical states
    bool fan = false;
    bool light = false;
    bool pump = false;
    bool valve = false;
    bool cooler = false;
    bool heater = false;
    bool phLowering = false;
    bool phRaising = false;
    bool phDosingEnabled = true;
    bool waterChange = false;
    bool sumpCleaning = false;
    bool sumpWaterValve = false;
    bool sumpDrainValve = false;

    // Control modes
    bool fanAuto = true;
    bool lightAuto = true;
    bool pumpAuto = true;
    bool valveAuto = true;
    bool coolerAuto = true;
    bool heaterAuto = true;
    bool phLoweringAuto = true;
    bool phRaisingAuto = true;
    bool waterChangeAuto = true;
    bool sumpCleaningAuto = true;
    bool sumpWaterValveAuto = true;
    bool sumpDrainValveAuto = true;

    // Emergency overrides
    bool emergencyMode = false;
    bool lowWaterEmergency = false;
    
    // Manual overrides
    bool fanManual = false;
    bool lightManual = false;
    bool pumpManual = false;
    bool valveManual = false;
    bool coolerManual = false;
    bool heaterManual = false;
    bool waterChangeManual = false;
    bool sumpCleaningManual = false;
    bool sumpWaterValveManual = false;
    bool sumpDrainValveManual = false;

    // Manual values (when manual mode is active)
    bool fanManualValue = false;
    bool lightManualValue = false;
    bool pumpManualValue = false;
    bool valveManualValue = false;
    bool coolerManualValue = false;
    bool heaterManualValue = false;
    bool waterChangeManualValue = false;
    bool sumpCleaningManualValue = false;
    bool sumpWaterValveManualValue = false;
    bool sumpDrainValveManualValue = false;

    bool fanAutoJustEnabled = false;
    bool lightAutoJustEnabled = false;
    bool pumpAutoJustEnabled = false;
    bool valveAutoJustEnabled = false;
    bool coolerAutoJustEnabled = false;
    bool heaterAutoJustEnabled = false;
    bool phDosingJustEnabled = false;
    bool waterChangeAutoJustEnabled = false;
    bool sumpCleaningAutoJustEnabled = false;
    bool sumpWaterValveAutoJustEnabled = false;
    bool sumpDrainValveAutoJustEnabled = false;
};

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
        bool cooler;
        bool heater;
        bool phLowering;
        bool phRaising;
        bool waterChange;
        bool sumpCleaning;
        bool sumpWaterValve;
        bool sumpDrainValve;

    } relayStates;
};
