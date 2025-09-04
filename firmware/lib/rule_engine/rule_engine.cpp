#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include <time.h>

unsigned long (*getMillis)() = millis;

#define PUMP_OVERRIDE_DURATION  300000UL
#define PUMP_OVERRIDE_COOLDOWN   60000UL

// === State Tracking ===
static bool fanActive = false;
static unsigned long fanOnSince = 0;
static bool overrideActive = false;
static unsigned long lastOverrideTime = 0;
static unsigned long lastPumpToggle = 0;
static bool pumpScheduledOn = false;

// === Helpers ===
inline unsigned long millis_elapsed(unsigned long now, unsigned long since) {
    return (unsigned long)(now - since);
}

// === FAN LOGIC ===
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes) {
    // ← ADDED: Sensor validation
    if (isnan(airTemp) || isnan(humidity)) {
        Serial.println("⚠️ Invalid sensor data for fan control");
        return;
    }

    const int MONITOR_START = 480;  // 08:00
    const int MONITOR_END   = 1020; // 17:00

    bool inWindow = currentMinutes >= MONITOR_START && currentMinutes <= MONITOR_END;
    unsigned long now = getMillis();

    bool tempHigh = airTemp >= TEMP_ON_THRESHOLD;
    bool humHigh = humidity > HUMIDITY_ON_THRESHOLD;
    bool emergency = airTemp >= TEMP_EMERGENCY;

    bool shouldTurnOn = inWindow && !fanActive && ((tempHigh && humHigh) || emergency);
    bool shouldTurnOff = fanActive && (
        (airTemp < TEMP_OFF_THRESHOLD && humidity < HUMIDITY_OFF_THRESHOLD) ||
        millis_elapsed(now, fanOnSince) >= FAN_MAX_CONTINUOUS_MS
    );

    if (shouldTurnOn) {
        fanActive = true;
        fanOnSince = now;
        control_fan(true);
        Serial.printf("🌀 Fan ON — Temp: %.1f°C, Humidity: %.1f%%\n", airTemp, humidity);
    } else if (shouldTurnOff && millis_elapsed(now, fanOnSince) >= FAN_MINUTE_RUNTIME) {
        fanActive = false;
        control_fan(false);
        Serial.println("✅ Fan OFF — Conditions normal or max runtime exceeded");
    }
}

// === PUMP LOGIC WITH FLOAT SWITCH ===
void check_and_control_pump(float waterTemp, bool waterLevelLow) {  // ← FIXED: Match header signature
    // ← ADDED: Sensor validation
    if (isnan(waterTemp)) {
        Serial.println("⚠️ Invalid water temperature for pump control");
        return;
    }

    unsigned long now = getMillis();

    // === Handle High Temp Override ===
    if (waterTemp > 30.0 && !overrideActive && millis_elapsed(now, lastOverrideTime) > PUMP_OVERRIDE_COOLDOWN) {
        overrideActive = true;
        lastOverrideTime = now;
        Serial.printf("⚠️ Pump override ON — water temp: %.1f°C\n", waterTemp);
    }
    if (overrideActive && millis_elapsed(now, lastOverrideTime) >= PUMP_OVERRIDE_DURATION) {
        overrideActive = false;
        Serial.println("✅ Pump override OFF — duration complete");
    }

    // === Float switch safety ===
    if (waterLevelLow) {
        control_pump(false);    // Always force OFF if water is low
        control_valve(true);    // Start refill valve
        Serial.println("🚰 Emergency refill active — water level low");
        return;
    }

    control_valve(false);       // Stop refill if water level is OK

    // === Normal Pump Cycle ===
    if (overrideActive) {
        control_pump(true);
        return;
    }

    // ← FIXED: Convert minutes to milliseconds
    if (!pumpScheduledOn && millis_elapsed(now, lastPumpToggle) >= (PUMP_OFF_DURATION * 60000UL)) {
        pumpScheduledOn = true;
        lastPumpToggle = now;
        control_pump(true);
        Serial.println("🔄 Pump cycle starting");
    } else if (pumpScheduledOn && millis_elapsed(now, lastPumpToggle) >= (PUMP_ON_DURATION * 60000UL)) {
        pumpScheduledOn = false;
        lastPumpToggle = now;
        control_pump(false);
        Serial.println("⏸️ Pump cycle pausing");
    }
}

// === LIGHT LOGIC ===
void check_and_control_light(const struct tm& now) {
    int mins = now.tm_hour * 60 + now.tm_min;
    bool on = (mins >= LIGHT_MORNING_ON && mins < LIGHT_MORNING_OFF) ||
              (mins >= LIGHT_EVENING_ON && mins < LIGHT_EVENING_OFF);
    control_light(on);
}

// === MAIN DISPATCH ===
void apply_rules(const RealTimeData& current, const struct tm& now) {
    int currentMinutes = now.tm_hour * 60 + now.tm_min;

    // ← FIXED: Use data from current instead of calling float_switch_active() twice
    bool waterLevelLow = current.floatTriggered;

    // Fan control
    check_climate_and_control_fan(current.airTemp, current.airHumidity, currentMinutes);

    // Pump + emergency refill
    check_and_control_pump(current.waterTemp, waterLevelLow);

    // Light control
    check_and_control_light(now);

}

// === EMERGENCY RULES (when time is not available) ===
void apply_emergency_rules(const RealTimeData& current) {
    // Only apply critical safety rules that don't require time
    bool waterLevelLow = current.floatTriggered;

    // Emergency pump control (float switch safety only)
    if (waterLevelLow) {
        control_pump(false);    // Always force OFF if water is low
        control_valve(true);    // Start refill valve
        Serial.println("🚰 Emergency refill active — water level low");
    } else {
        control_valve(false);   // Stop refill if water level is OK
    }
}
