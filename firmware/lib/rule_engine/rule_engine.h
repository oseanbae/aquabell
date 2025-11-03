#pragma once
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h"
#include "firebase.h"

// Initialize relays (implementation expected elsewhere)
void relay_control_init();

// New mode-aware rule engine that handles AUTO/MANUAL mode control
void applyRulesWithModeControl(RealTimeData& data, ActuatorState& actuators, const Commands& commands, unsigned long nowMillis);

// Emergency fallback functions
void checkValveLogic(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis);
void checkPumpLogic(ActuatorState& actuators, unsigned long nowMillis);
void checkFanLogic(ActuatorState& actuators, float airTemp, float humidity, unsigned long nowMillis);
void checkLightLogic(ActuatorState& actuators, unsigned long nowMillis);
void checkWaterCoolerLogic(ActuatorState& actuators, float waterTemp, unsigned long nowMillis);

// Main rule evaluation entry point
void evaluateRules(bool forceImmediate = false);