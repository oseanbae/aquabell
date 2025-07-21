#include <Arduino.h>
#include "config.h"
#include "actuator_state.h"

ActuatorState actuatorState;

// Relay pins array in same order as state fields
const int RELAYS[] = {
    FAN_RELAY_PIN,
    LIGHT_RELAY_PIN,
    PUMP_RELAY_PIN,
    VALVE_RELAY_PIN,
    AIR_RELAY_PIN
};

// === Init all relays ===
void relay_control_init() {
    for (int i = 0; i < sizeof(RELAYS) / sizeof(RELAYS[0]); i++) {
        pinMode(RELAYS[i], OUTPUT);
        // Turn OFF all except AIR relay
        bool initialState = (RELAYS[i] == AIR_RELAY_PIN);
        digitalWrite(RELAYS[i], initialState ? HIGH : LOW);

        // Set actuator state
        if (RELAYS[i] == FAN_RELAY_PIN) actuatorState.fan = initialState;
        else if (RELAYS[i] == LIGHT_RELAY_PIN) actuatorState.light = initialState;
        else if (RELAYS[i] == PUMP_RELAY_PIN) actuatorState.pump = initialState;
        else if (RELAYS[i] == VALVE_RELAY_PIN) actuatorState.valve = initialState;
        else if (RELAYS[i] == AIR_RELAY_PIN) actuatorState.air = initialState;
    }

    Serial.println("✅ Relay control initialized. AIR pump is ON.");
}

// === Relay control logic ===
void setRelay(int relayPin, bool state) {
    digitalWrite(relayPin, state ? HIGH : LOW);

    if (relayPin == FAN_RELAY_PIN) actuatorState.fan = state;
    else if (relayPin == PUMP_RELAY_PIN) actuatorState.pump = state;
    else if (relayPin == LIGHT_RELAY_PIN) actuatorState.light = state;
    else if (relayPin == VALVE_RELAY_PIN) actuatorState.valve = state;
    else if (relayPin == AIR_RELAY_PIN) actuatorState.air = state;
}

// === Component-specific wrappers ===
void control_fan(bool state)   { setRelay(FAN_RELAY_PIN, state); }
void control_light(bool state) { setRelay(LIGHT_RELAY_PIN, state); }
void control_pump(bool state)  { setRelay(PUMP_RELAY_PIN, state); }
void control_valve(bool state) { setRelay(VALVE_RELAY_PIN, state); }
void control_air(bool state)   { setRelay(AIR_RELAY_PIN, state); }
