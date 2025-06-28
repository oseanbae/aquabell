#include "Arduino.h"
#include "sensor_config.h"

static bool lastState = HIGH;
static unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // ms

void float_switch_init() {
    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);
}

// Detects if the float switch was just pressed (LOW transition)
bool is_float_switch_triggered() {
    int current = digitalRead(FLOAT_SWITCH_PIN);
    unsigned long now = millis();
    bool triggered = false;

    if (current != lastState && (now - lastDebounceTime > debounceDelay)) {
        lastDebounceTime = now;
        if (current == LOW && lastState == HIGH) {
            triggered = true;
        }
    }

    lastState = current;
    return triggered;
}

// Returns true if the float switch is currently pressed (LOW)
bool float_switch_active() {
    return digitalRead(FLOAT_SWITCH_PIN) == LOW;
}
