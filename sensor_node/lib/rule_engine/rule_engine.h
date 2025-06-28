#pragma once
#include "sensor_data.h"
#include "sensor_config.h"

void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes);
void check_and_control_pump();
void check_and_control_light();
