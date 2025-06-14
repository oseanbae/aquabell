#include <Arduino.h>
#include "config.h"


void float_switch_init() {
    pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP); // Initialize the float switch pin as input with pull-up resistor
}

bool lastState = HIGH;

void float_switch_read() {
    int currentState = digitalRead(FLOAT_SWITCH_PIN);

    if (currentState == LOW && lastState == HIGH) {
        Serial.println("⚠️ Button just pressed (float switch triggered)");
        // Here you can add the code to handle the float switch being triggered
    }

    lastState = currentState;
}