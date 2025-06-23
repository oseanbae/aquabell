#pragma once
#include <Arduino.h>

struct DeviceState {
    bool fan = false;
    bool light = false;
    bool pump = false;
    bool air = false;
    bool valve = false;
};

void relay_control_init();
void setRelay(int relayPin, bool state);
void control_fan(bool state);
void control_light(bool state);
void control_pump(bool state);
void control_air(bool state);
void control_valve(bool state);