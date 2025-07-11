#include "Arduino.h"
#include "config.h"  // defines FLOAT_SWITCH_PIN
#include "float_switch.h"

// Debounce state tracking
static bool lastStableState = HIGH;
static bool lastRawState = HIGH;
static unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // milliseconds

void float_switch_init() {
    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP); // Assumes float switch pulls LOW when water is low
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);     // start off
    digitalWrite(BUZZER_PIN, LOW);
}

// Returns true if float switch is currently LOW (water is low)
bool float_switch_active() {
    return digitalRead(FLOAT_SWITCH_PIN) == LOW;
}

// Returns true once when float changes from HIGH → LOW (water just became low)
bool is_float_switch_triggered() {
    bool triggered = false;
    int reading = digitalRead(FLOAT_SWITCH_PIN);
    unsigned long now = millis();

    // Debounce logic
    if (reading != lastRawState) {
        lastDebounceTime = now;
        lastRawState = reading;
    }

    if ((now - lastDebounceTime) > debounceDelay) {
        if (reading != lastStableState) {
            if (lastStableState == HIGH && reading == LOW) {
                triggered = true; // Transition HIGH → LOW
            }
            lastStableState = reading;
        }
    }

    return triggered;
}
