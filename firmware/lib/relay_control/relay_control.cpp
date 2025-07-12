#include <Arduino.h>
#include "config.h"
#include "actuator_state.h"

// Use the pin macros from config.h to create the relay array
const int RELAYS[] = {
    FAN_RELAY_PIN,
    LIGHT_RELAY_PIN,
    PUMP_RELAY_PIN,
    VALVE_RELAY_PIN
};  

void relay_control_init() {
    for (int i = 0; i < sizeof(RELAYS) / sizeof(RELAYS[0]); i++) {
        pinMode(RELAYS[i], OUTPUT);
        digitalWrite(RELAYS[i], LOW); // Turn off all relays
    }
}


void setRelay(int relayPin, bool state) {
    digitalWrite(relayPin, state ? HIGH : LOW);
    if (relayPin == FAN_RELAY_PIN) actuatorState.fan = state;
    else if (relayPin == PUMP_RELAY_PIN) actuatorState.pump = state;
    else if (relayPin == LIGHT_RELAY_PIN) actuatorState.light = state;
    else if (relayPin == VALVE_RELAY_PIN) actuatorState.valve = state;
}

void control_fan(bool state)   { setRelay(RELAYS[0], state); }
void control_light(bool state) { setRelay(RELAYS[1], state); }
void control_pump(bool state)  { setRelay(RELAYS[2], state); }
void control_valve(bool state) { setRelay(RELAYS[3], state); }
