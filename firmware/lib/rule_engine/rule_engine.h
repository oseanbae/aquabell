#pragma once
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h"
#include "firestore_client.h"

// Initialize relays (implementation expected elsewhere)
void relay_control_init();

// Actuator control checks (manual + auto)
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes, const CommandState& cmd);
void check_and_control_pump(bool waterLevelLow, const CommandState& cmd, const struct tm& now);
void check_and_control_light(const struct tm& now, const CommandState& cmd);
void check_and_control_valve(bool waterLevelLow, const CommandState& cmd, const struct tm& now);

// Dispatcher — applies all rules in one pass
void apply_rules(const RealTimeData& current, const struct tm& now, const Commands& commands);
