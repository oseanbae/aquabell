#include <Arduino.h>
#include "config.h"
#include "RTClib.h"

#define PUMP_ON_DURATION 15      // Pump ON duration in minutes
#define PUMP_OFF_DURATION 45     // Pump OFF duration in minutes
#define PUMP_CYCLE_DURATION (PUMP_ON_DURATION + PUMP_OFF_DURATION) // Total cycle duration in minutes

#define LIGHT_MORNING_ON 360    // 6 * 60 = 6:00 AM
#define LIGHT_MORNING_OFF 600   // 10 * 60 = 10:00 AM
#define LIGHT_EVENING_ON 930    // 15 * 60 + 30 = 15:30 (3:30 PM)
#define LIGHT_EVENING_OFF 1170  // 19 * 60 + 30 = 19:30 (7:30 PM)


RTC_DS3231 rtc; // Create an RTC object

// Use the pin macros from config.h to create the relay array
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

void check_and_control_pump() {
    DateTime now = rtc.now();
    int minutes = now.minute();
    
    if ((minutes % PUMP_CYCLE_DURATION) < PUMP_ON_DURATION) {
       control_pump(true); // ON
    } else {
        control_pump(false); // OFF
    }
}

void check_and_control_light() {
    DateTime now = rtc.now();
    int currentMinutes = now.hour() * 60 + now.minute();

    bool shouldBeOn = (currentMinutes >= LIGHT_MORNING_ON && currentMinutes < LIGHT_MORNING_OFF) ||
                      (currentMinutes >= LIGHT_EVENING_ON && currentMinutes < LIGHT_EVENING_OFF);

    control_light(shouldBeOn);
}

void setRelay(int relayPin, bool state) {digitalWrite(relayPin, state ? HIGH : LOW);}
void control_fan(bool state) {setRelay(0, state);}
void control_light(bool state) {setRelay(1, state);}
void control_pump(bool state) {setRelay(2, state);}
void control_air(bool state) {setRelay(3, state);}
void control_valve(bool state) {setRelay(4, state);}