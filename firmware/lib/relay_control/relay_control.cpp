#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"

extern ActuatorState actuators;

// Relay pins array in same order as state fields
const int RELAYS[] = {
    FAN_RELAY_PIN,
    LIGHT_RELAY_PIN,
    PUMP_RELAY_PIN,
    VALVE_RELAY_PIN,
    WATER_COOLER_RELAY_PIN,
    WATER_HEATER_RELAY_PIN
};

// === Init all relays ===
void relay_control_init() {
    for (int i = 0; i < sizeof(RELAYS) / sizeof(RELAYS[0]); i++) {
        pinMode(RELAYS[i], OUTPUT);
        // Turn OFF all relays at startup. RELAYS MUST BE WIRES TO NO TERMIALS
        digitalWrite(RELAYS[i], HIGH); //HIGH = OFF for active LOW relays, LOW = ON relay

        // Set actuator state
        if (RELAYS[i] == FAN_RELAY_PIN) actuators.fan = false;
        else if (RELAYS[i] == LIGHT_RELAY_PIN) actuators.light = false;
        else if (RELAYS[i] == PUMP_RELAY_PIN) actuators.pump = false;
        else if (RELAYS[i] == VALVE_RELAY_PIN) actuators.valve = false;
        else if (RELAYS[i] == WATER_COOLER_RELAY_PIN) actuators.waterCooler = false;
        else if (RELAYS[i] == WATER_HEATER_RELAY_PIN) actuators.waterHeater = false;
    }

    Serial.println("✅ Relay control initialized. All relays OFF.");
}

// === Relay control logic ===
void setRelay(int relayPin, bool state) {
    digitalWrite(relayPin, state ? LOW : HIGH); 
    // if state == true → relayPin gets LOW (relay ON, since active LOW)
    // if state == false → relayPin gets HIGH (relay OFF)

    if (relayPin == FAN_RELAY_PIN) actuators.fan = state;
    else if (relayPin == PUMP_RELAY_PIN) actuators.pump = state;
    else if (relayPin == LIGHT_RELAY_PIN) actuators.light = state;
    else if (relayPin == VALVE_RELAY_PIN) actuators.valve = state;
    else if (relayPin == WATER_COOLER_RELAY_PIN) actuators.waterCooler = state;
    else if (relayPin == WATER_HEATER_RELAY_PIN) actuators.waterHeater = state;
}

// === Component-specific wrappers ===
void control_fan(bool state)   { setRelay(FAN_RELAY_PIN, state); }
void control_light(bool state) { setRelay(LIGHT_RELAY_PIN, state); }
void control_pump(bool state)  { setRelay(PUMP_RELAY_PIN, state); }
void control_valve(bool state) { setRelay(VALVE_RELAY_PIN, state); }
void control_waterCooler(bool state) { setRelay(WATER_COOLER_RELAY_PIN, state); }
void control_waterHeater(bool state) { setRelay(WATER_HEATER_RELAY_PIN, state); }