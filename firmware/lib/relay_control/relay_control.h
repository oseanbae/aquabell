#pragma once
#include <Arduino.h>

void relay_control_init();
void control_fan(bool state);
void control_light(bool state);
void control_pump(bool state);
void control_valve(bool state);
void control_cooler(bool state);
void control_heater(bool state);
void control_ph_pump(bool up, bool down);
void control_drain(bool state);
void control_sump_cleaning(bool waterValve, bool drainValve);
