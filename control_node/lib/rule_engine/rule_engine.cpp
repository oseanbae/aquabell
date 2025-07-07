#include <Arduino.h>
#include "control_config.h"
#include "RTClib.h"
#include "relay_control.h"
#include "sensor_data.h"
#include "esp_task_wdt.h"  // ESP32 Watchdog

#define WDT_TIMEOUT_SECONDS 5

RTC_DS3231 rtc;

bool fanActive = false;
unsigned long fanOnSince = 0;

inline unsigned long millis_elapsed(unsigned long current, unsigned long since) {
    return (unsigned long)(current - since); // Handles millis rollover
}

// ==== FAN LOGIC WITH HYSTERESIS + SAFEGUARDS ====
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes) {
    const int MONITOR_START = 480;   // 8:00 AM
    const int MONITOR_END   = 1020;  // 5:00 PM

    bool inMonitoringWindow = currentMinutes >= MONITOR_START && currentMinutes <= MONITOR_END;
    unsigned long now = millis();

    bool shouldTurnOn = inMonitoringWindow &&
                        !fanActive &&
                        ((airTemp >= TEMP_ON_THRESHOLD && humidity > HUMIDITY_ON_THRESHOLD) ||
                         airTemp >= TEMP_EMERGENCY);

    bool shouldTurnOff = fanActive &&
                         ((airTemp < TEMP_OFF_THRESHOLD && humidity < HUMIDITY_OFF_THRESHOLD) ||
                          millis_elapsed(now, fanOnSince) >= FAN_MAX_CONTINUOUS_MS);

    if (shouldTurnOn) {
        fanActive = true;
        fanOnSince = now;
        control_fan(true);
        Serial.println("🌀 Fan ON — Triggered by climate or emergency");
    } 
    else if (shouldTurnOff && millis_elapsed(now, fanOnSince) >= FAN_MINUTE_RUNTIME) {
        fanActive = false;
        control_fan(false);
        Serial.println("✅ Fan OFF — Conditions normal or max runtime exceeded");
    }
}

// ==== PUMP LOGIC WITH SAFE OVERRIDE ====
void check_and_control_pump(DateTime now, float waterTemp) {
    static unsigned long lastOverrideTime = 0;
    static bool overrideActive = false;

    int totalMinutes = now.hour() * 60 + now.minute();
    bool inScheduledOn = (totalMinutes % PUMP_CYCLE_DURATION) < PUMP_ON_DURATION;

    unsigned long currentMillis = millis();

    if  (waterTemp > 30.0 && !overrideActive && millis_elapsed(currentMillis, lastOverrideTime) > 60000) {
        overrideActive = true;
        lastOverrideTime = currentMillis;
        Serial.println("⚠️ Temp > 30°C — forcing pump ON for cooling");
    }

    const unsigned long OVERRIDE_DURATION = 300000;  // 5 mins

    if (overrideActive && millis_elapsed(currentMillis, lastOverrideTime) >= OVERRIDE_DURATION) {
        overrideActive = false;
        Serial.println("✅ Pump override complete");
    }

    control_pump(inScheduledOn || overrideActive);
}

// ==== LIGHT WINDOW CONTROL ====
void check_and_control_light(DateTime now) {
    int currentMinutes = now.hour() * 60 + now.minute();

    bool shouldBeOn = (currentMinutes >= LIGHT_MORNING_ON && currentMinutes < LIGHT_MORNING_OFF) ||
                      (currentMinutes >= LIGHT_EVENING_ON && currentMinutes < LIGHT_EVENING_OFF);

    control_light(shouldBeOn);
}

// ==== CENTRAL RULE DISPATCH ====
void apply_rules(SensorData &current) {
    DateTime now = rtc.now();
    int currentMinutes = now.hour() * 60 + now.minute();

    check_climate_and_control_fan(current.airTemp, current.airHumidity, currentMinutes);
    check_and_control_pump(now, current.waterTemp);
    check_and_control_light(now);
}

// // ==== SETUP ====
// void setup() {
//     Serial.begin(115200);
//     relay_control_init();
//     rtc.begin();

//     // Watchdog init
//     esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);  // Enable panic/reboot
//     esp_task_wdt_add(NULL);  // Add main task to watchdog

//     Serial.println("🚀 System initialized");
// }

// // Dummy sensor object (replace with actual sensor reading logic)
// SensorData currentSensorData;

// // ==== LOOP ====
// void loop() {
//     // TODO: Read sensors here (mocking for now)
//     currentSensorData.airTemp = 27.5;
//     currentSensorData.airHumidity = 78.0;
//     currentSensorData.waterTemp = 29.0;

//     apply_rules(currentSensorData);

//     esp_task_wdt_reset();  // Reset watchdog
//     delay(1000);           // Main loop sleep
// }
