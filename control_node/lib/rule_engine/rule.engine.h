#pragma once 
#include <Arduino.h>
#include "RTClib.h"

void relay_control_init();
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes);
void check_and_control_pump(DateTime now, float waterTemp);
void check_and_control_light(DateTime now);