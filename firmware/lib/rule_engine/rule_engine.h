#pragma once
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h"
#include "firestore_client.h"

// Initialize relays (implementation expected elsewhere)
void relay_control_init();

// Actuator control checks (manual + auto) - Time-aware versions
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes, const CommandState& cmd);
void check_and_control_pump(bool waterLevelLow, const CommandState& cmd, const struct tm& now);
void check_and_control_light(const struct tm& now, const CommandState& cmd);
void check_and_control_valve(bool waterLevelLow, const CommandState& cmd, const struct tm& now);

// Fallback versions for when time is not available (millis-based)
void check_climate_and_control_fan_fallback(float airTemp, float humidity, const CommandState& cmd);
void check_and_control_pump_fallback(bool waterLevelLow, const CommandState& cmd, unsigned long nowMillis);
void check_and_control_light_fallback(const CommandState& cmd);
void check_and_control_valve_fallback(bool waterLevelLow, const CommandState& cmd, unsigned long nowMillis);

// Main dispatcher functions
void updateActuators(const RealTimeData& current, Commands& commands, unsigned long nowMillis);
void apply_rules(const RealTimeData& current, const struct tm& now, const Commands& commands);
