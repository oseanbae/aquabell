#pragma once
#include <Arduino.h>

extern bool mockFloatSwitchState;

void control_fan(bool state) {
    Serial.printf("[MOCK] control_fan(%s)\n", state ? "ON" : "OFF");
}

void control_pump(bool state) {
    Serial.printf("[MOCK] control_pump(%s)\n", state ? "ON" : "OFF");
}

void control_valve(bool state) {
    Serial.printf("[MOCK] control_valve(%s)\n", state ? "ON" : "OFF");
}

void control_light(bool state) {
    Serial.printf("[MOCK] control_light(%s)\n", state ? "ON" : "OFF");
}

bool is_float_switch_triggered() {
    return mockFloatSwitchState;
}

void tone(int pin, int freq, int duration) {
    Serial.printf("[MOCK] tone(pin=%d, freq=%d, duration=%d)\n", pin, freq, duration);
}
