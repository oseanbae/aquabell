#include <Arduino.h>
#include "config.h"
#include "actuator_state.h"

ActuatorState actuatorState;

// Relay pins array in same order as state fields
const int RELAYS[] = {
    FAN_RELAY_PIN,
    LIGHT_RELAY_PIN,
    PUMP_RELAY_PIN,
    VALVE_RELAY_PIN
};

// === Init all relays ===
void relay_control_init() {
    for (int i = 0; i < sizeof(RELAYS) / sizeof(RELAYS[0]); i++) {
        pinMode(RELAYS[i], OUTPUT);
        // Turn OFF all relays at startup. RELAYS MUST BE WIRES TO NO TERMIALS
        digitalWrite(RELAYS[i], HIGH); //HIGH = OFF for active LOW relays, LOW = ON relay

        // Set actuator state
        if (RELAYS[i] == FAN_RELAY_PIN) actuatorState.fan = false;
        else if (RELAYS[i] == LIGHT_RELAY_PIN) actuatorState.light = false;
        else if (RELAYS[i] == PUMP_RELAY_PIN) actuatorState.pump = false;
        else if (RELAYS[i] == VALVE_RELAY_PIN) actuatorState.valve = false;
    }

    Serial.println("✅ Relay control initialized. All relays OFF.");
}

// === Relay control logic ===
void setRelay(int relayPin, bool state) {
    digitalWrite(relayPin, state ? LOW : HIGH); 
    // if state == true → relayPin gets LOW (relay ON, since active LOW)
    // if state == false → relayPin gets HIGH (relay OFF)

    if (relayPin == FAN_RELAY_PIN) actuatorState.fan = state;
    else if (relayPin == PUMP_RELAY_PIN) actuatorState.pump = state;
    else if (relayPin == LIGHT_RELAY_PIN) actuatorState.light = state;
    else if (relayPin == VALVE_RELAY_PIN) actuatorState.valve = state;
}

// === Component-specific wrappers ===
void control_fan(bool state)   { setRelay(FAN_RELAY_PIN, state); }
void control_light(bool state) { setRelay(LIGHT_RELAY_PIN, state); }
void control_pump(bool state)  { setRelay(PUMP_RELAY_PIN, state); }
void control_valve(bool state) { setRelay(VALVE_RELAY_PIN, state); }
