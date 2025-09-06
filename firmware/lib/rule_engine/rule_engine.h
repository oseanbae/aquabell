#pragma once 
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h" 
#include "firestore_client.h"

void relay_control_init();
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes, const CommandState& fanCommand);
void check_and_control_pump(float waterTemp, bool waterLevelLow, const CommandState& pumpCommand);
void check_and_control_light(const struct tm& now, const CommandState& lightCommand);
void check_and_control_valve(bool waterLevelLow, const CommandState& valveCommand);
void apply_rules(const RealTimeData& current, const struct tm& now, const Commands& commands);
void apply_emergency_rules(const RealTimeData& current);