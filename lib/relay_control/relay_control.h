#pragma once
#include <Arduino.h>

void relay_control_init();
void control_fan(bool state);
void control_light(bool state);
void control_pump(bool state);
void control_valve(bool state);