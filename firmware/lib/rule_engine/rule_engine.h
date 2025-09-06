#pragma once 
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h" 

// Relay state structure for RTDB
struct RelayState {
    bool isAuto;
    bool value;
};

struct RelayStates {
    RelayState fan;
    RelayState light;
    RelayState pump;
    RelayState valve;
};

void relay_control_init();
bool fetchRelayStates(RelayStates& states);
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes, const RelayState& fanState);
void check_and_control_pump(float waterTemp, bool waterLevelLow, const RelayState& pumpState);
void check_and_control_light(const struct tm& now, const RelayState& lightState);
void check_and_control_valve(bool waterLevelLow, const RelayState& valveState);
void apply_rules(const RealTimeData& current, const struct tm& now);
void apply_emergency_rules(const RealTimeData& current);