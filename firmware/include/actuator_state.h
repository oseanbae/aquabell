#pragma once 
#include <Arduino.h>

struct ActuatorStates {
    bool fan;
    bool pump;
    bool light;
    bool valve;
} actuatorState;

extern ActuatorStates actuatorState;