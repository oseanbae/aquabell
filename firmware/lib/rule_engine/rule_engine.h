#pragma once 
#include <Arduino.h>
#include <time.h>
#include "sensor_data.h" 

void relay_control_init();
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes);
void check_and_control_pump(float waterTemp, bool waterLevelLow);
void check_and_control_light(const struct tm& now); 
void apply_rules(const RealTimeData& current, const struct tm& now);
void apply_emergency_rules(const RealTimeData& current);