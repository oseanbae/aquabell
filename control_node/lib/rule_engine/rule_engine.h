#pragma once
#include "sensor_data.h"
#include <RTClib.h>

void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes);
void check_and_control_pump(DateTime now, float waterTemp);
void check_and_control_light(DateTime now);