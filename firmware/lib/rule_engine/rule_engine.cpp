#include <Arduino.h>
#include "config.h"
#include "RTClib.h"
#include "relay_control.h"
#include "sensor_data.h"

RTC_DS3231 rtc;


bool fanActive = false;
unsigned long fanOnSince = 0;

// ==== MAIN CHECK FUNCTION ====
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes) {
    const int MONITOR_START = 480;   // 8:00 AM
    const int MONITOR_END   = 1020;  // 5:00 PM

    bool inMonitoringWindow = currentMinutes >= MONITOR_START && currentMinutes <= MONITOR_END;

    // Core logic: trigger only if both temp and humidity are high
    bool normalTrigger    = inMonitoringWindow && (airTemp >= TEMP_THRESHOLD && humidity > HUMIDITY_THRESHOLD);
    bool emergencyTrigger = inMonitoringWindow && (airTemp >= TEMP_EMERGENCY);  // force fan even if RH is low

    if ((normalTrigger || emergencyTrigger)) {
        if (!fanActive) {
            fanActive = true;
            fanOnSince = millis();
            control_fan(true);
            Serial.println("🌀 Fan ON — Temp/Humidity threshold exceeded");
        }
    } 
    else {
        if (fanActive && (millis() - fanOnSince >= FAN_MINUTE_RUNTIME)) {
            fanActive = false;
            control_fan(false);
            Serial.println("✅ Fan OFF — Runtime complete and conditions normal");
        }
    }
}

//Scheduled water pump 15mins ON - 45mins OFF continously
void check_and_control_pump() {
    DateTime now = rtc.now();
    int totalMinutes = now.hour() * 60 + now.minute();

    if ((totalMinutes % PUMP_CYCLE_DURATION) < PUMP_ON_DURATION) {
        control_pump(true);  // ON
    } else {
        control_pump(false); // OFF
    }
}


//Schedule grow light 5:30am - 9:00am & 3:00pm - 6:00pm
void check_and_control_light() {
    DateTime now = rtc.now();
    int currentMinutes = now.hour() * 60 + now.minute();

    bool shouldBeOn = (currentMinutes >= LIGHT_MORNING_ON && currentMinutes < LIGHT_MORNING_OFF) ||
                      (currentMinutes >= LIGHT_EVENING_ON && currentMinutes < LIGHT_EVENING_OFF);

    control_light(shouldBeOn);
}
