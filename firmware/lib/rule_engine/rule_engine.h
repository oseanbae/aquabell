#pragma once
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h"
#include "firebase.h"

// Initialize relays (implementation expected elsewhere)
void relay_control_init();

// New centralized rule engine with ActuatorState
void applyRules(RealTimeData& data, ActuatorState& actuators, unsigned long nowMillis);

// New mode-aware rule engine that handles AUTO/MANUAL mode control
void applyRulesWithModeControl(RealTimeData& data, ActuatorState& actuators, const Commands& commands, unsigned long nowMillis);

// Emergency fallback functions
void checkValveLogic(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis);
void checkPumpLogic(ActuatorState& actuators, unsigned long nowMillis);
void checkFanLogic(ActuatorState& actuators, float airTemp, float humidity, unsigned long nowMillis);
void checkLightLogic(ActuatorState& actuators, unsigned long nowMillis);

// Legacy functions (kept for compatibility)
void updateActuators(RealTimeData& current, Commands& commands, unsigned long nowMillis);
void apply_rules(RealTimeData& current, const struct tm& now, const Commands& commands);

void evaluateRules(bool forceImmediate = false);