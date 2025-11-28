#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"

extern ActuatorState actuators;

// Struct to map relay → actuator boolean
struct RelayMap {
    int pin;
    bool* statePtr;
};

// === Relay mapping table ===
RelayMap RELAY_TABLE[] = {
    { FAN_RELAY_PIN,              &actuators.fan },
    { LIGHT_RELAY_PIN,            &actuators.light },
    { PUMP_RELAY_PIN,             &actuators.pump },
    { VALVE_RELAY_PIN,            &actuators.valve },
    { WATER_COOLER_RELAY_PIN,     &actuators.cooler },
    { WATER_HEATER_RELAY_PIN,     &actuators.heater },
    { PH_LOWERING_RELAY_PIN,      &actuators.phLowering },
    { PH_RAISING_RELAY_PIN,       &actuators.phRaising },
    { DRAIN_PUMP_RELAY_PIN,       &actuators.waterChange },
    { FLUSH_VALVE_RELAY_PIN,      &actuators.sumpWaterValve },
    { DRAIN_VALVE_RELAY_PIN,      &actuators.sumpDrainValve },
};

const int RELAY_COUNT = sizeof(RELAY_TABLE) / sizeof(RELAY_TABLE[0]);

// === Init all relays ===
void relay_control_init() {
    for (int i = 0; i < RELAY_COUNT; i++) {
        pinMode(RELAY_TABLE[i].pin, OUTPUT);
        digitalWrite(RELAY_TABLE[i].pin, HIGH);  // ACTIVE LOW: HIGH = OFF
        *(RELAY_TABLE[i].statePtr) = false;
    }

    Serial.println("✅ Relay control initialized. All relays OFF.");
}

// === Unified relay control ===
void setRelay(int relayPin, bool state) {
    digitalWrite(relayPin, state ? LOW : HIGH);

    for (int i = 0; i < RELAY_COUNT; i++) {
        if (RELAY_TABLE[i].pin == relayPin) {
            *(RELAY_TABLE[i].statePtr) = state;
            break;
        }
    }
}

// === Component wrappers ===
void control_fan(bool s)               { setRelay(FAN_RELAY_PIN, s); }
void control_light(bool s)             { setRelay(LIGHT_RELAY_PIN, s); }
void control_pump(bool s)              { setRelay(PUMP_RELAY_PIN, s); }
void control_valve(bool s)             { setRelay(VALVE_RELAY_PIN, s); }
void control_cooler(bool s)            { setRelay(WATER_COOLER_RELAY_PIN, s); }
void control_heater(bool s)            { setRelay(WATER_HEATER_RELAY_PIN, s); }

void control_ph_pump(bool up, bool down) {
    setRelay(PH_LOWERING_RELAY_PIN, down);
    setRelay(PH_RAISING_RELAY_PIN, up);
}

void control_drain(bool s)             { setRelay(DRAIN_PUMP_RELAY_PIN, s); }


void control_sump_cleaning(bool waterValve, bool drainValve) {
    setRelay(FLUSH_VALVE_RELAY_PIN, waterValve);
    setRelay(DRAIN_VALVE_RELAY_PIN, drainValve);
}
