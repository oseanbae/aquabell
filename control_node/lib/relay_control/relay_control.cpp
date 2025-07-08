#include <Arduino.h>
#include "control_config.h"


// Use the pin macros from sensor_config.h to create the relay array
const int RELAYS[] = {
    FAN_RELAY_PIN,
    LIGHT_RELAY_PIN,
    PUMP_RELAY_PIN,
    AIR_RELAY_PIN,
    VALVE_RELAY_PIN
};  

void relay_control_init() {
    for (int i = 0; i < sizeof(RELAYS) / sizeof(RELAYS[0]); i++) {
        pinMode(RELAYS[i], OUTPUT);
        digitalWrite(RELAYS[i], LOW); // Turn off all relays
    }
}


void setRelay(int relayPin, bool state) {digitalWrite(relayPin, state ? HIGH : LOW);}
void control_fan(bool state)   { setRelay(RELAYS[0], state); }
void control_light(bool state) { setRelay(RELAYS[1], state); }
void control_pump(bool state)  { setRelay(RELAYS[2], state); }
void control_air(bool state)   { setRelay(RELAYS[3], state); }
void control_valve(bool state) { setRelay(RELAYS[4], state); }
