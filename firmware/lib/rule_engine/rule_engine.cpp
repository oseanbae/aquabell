#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include "firestore_client.h"
#include <time.h>

// ===== Utility: millis tracking =====
inline unsigned long millis_elapsed(unsigned long now, unsigned long since) {
    return now - since;
}

// ===== Utility: Manual override =====
bool handle_manual_mode(const CommandState& cmd, void (*controlFn)(bool), const char* label) {
    if (!cmd.isAuto) {
        controlFn(cmd.value);
        Serial.printf("%s MANUAL — %s\n", label, cmd.value ? "ON" : "OFF");
        return true;
    }
    return false;
}

struct ValveState {
    bool open = false;
    unsigned long openSince = 0;
    unsigned long floatLowSince = 0;
    unsigned long floatHighSince = 0;
};
static ValveState valveState;

// ===== FAN LOGIC =====
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes, const CommandState& cmd) {
    if (handle_manual_mode(cmd, control_fan, "🌀 Fan")) return;
    if (isnan(airTemp) || isnan(humidity)) return;

    static bool fanActive = false;
    static unsigned long fanOnSince = 0;
    unsigned long now = millis();

    // Schedule window (daytime 08:00–17:00)
    const int DAY_START = 480;   // 08:00
    const int DAY_END   = 1020;  // 17:00
    bool isDaytime = currentMinutes >= DAY_START && currentMinutes <= DAY_END;

    // Thresholds
    bool tempHigh = airTemp >= TEMP_ON_THRESHOLD;
    bool tempLow  = airTemp < TEMP_OFF_THRESHOLD;
    bool humHigh  = humidity > HUMIDITY_ON_THRESHOLD;
    bool humLow   = humidity < HUMIDITY_OFF_THRESHOLD;
    bool emergency = airTemp >= TEMP_EMERGENCY;

    // Decisions
    bool shouldOn = false;
    bool shouldOff = false;

    if (isDaytime) {
        // Daytime: respond to temp, humidity, or emergency
        shouldOn  = (!fanActive) && (emergency || tempHigh || humHigh);
        shouldOff = fanActive && tempLow && humLow;
    } else {
        // Nighttime: ignore humidity, only emergency temp
        shouldOn  = (!fanActive) && emergency;
        shouldOff = fanActive && tempLow;
    }

    // Safety: max runtime
    if (fanActive && millis_elapsed(now, fanOnSince) >= FAN_MAX_CONTINUOUS_MS) {
        shouldOff = true;
    }

    // Apply state
    if (shouldOn) {
        fanActive = true;
        fanOnSince = now;
        control_fan(true);
        Serial.printf("🌀 Fan AUTO ON — T=%.1f°C, H=%.1f%% (Day: %s)\n",
                      airTemp, humidity, isDaytime ? "YES" : "NO");
    } 
    else if (shouldOff && millis_elapsed(now, fanOnSince) >= FAN_MINUTE_RUNTIME) {
        fanActive = false;
        control_fan(false);
        Serial.println("✅ Fan AUTO OFF — Normalized or safety limit");
    }
}


// ===== PUMP LOGIC =====
void check_and_control_pump(bool waterLevelLow, const CommandState& cmd, const struct tm& now) {
    if (handle_manual_mode(cmd, control_pump, "🔄 Pump")) return;

    unsigned long ms = millis();
    static bool pumpOn = false;
    static unsigned long lastToggle = 0;

    // 🚰 Safety: Float switch
    if (waterLevelLow) {
        if (pumpOn) {
            unsigned long runtimeMin = (ms - lastToggle) / 60000UL;
            control_pump(false);
            pumpOn = false;
            Serial.printf("🚰 Pump OFF — water level low (was ON for %lu min, %02d:%02d)\n",
                          runtimeMin, now.tm_hour, now.tm_min);
        }
        return; // exit early until water is safe again
    }

    // First run: start pump ON
    if (lastToggle == 0) {
        pumpOn = true;
        lastToggle = ms;
        control_pump(true);
        Serial.printf("🔄 Pump cycle START — ON at %02d:%02d\n", now.tm_hour, now.tm_min);
        return;
    }

    // Track elapsed minutes since last toggle
    unsigned long elapsedMin = (ms - lastToggle) / 60000UL;

    // Scheduled cycle logic
    if (pumpOn && millis_elapsed(ms, lastToggle) >= (PUMP_ON_DURATION * 60000UL)) {
        pumpOn = false;
        lastToggle = ms;
        control_pump(false);
        Serial.printf("⏸️ Pump cycle OFF — ran %lu min, now %02d:%02d\n",
                      elapsedMin, now.tm_hour, now.tm_min);
    } 
    else if (!pumpOn && millis_elapsed(ms, lastToggle) >= (PUMP_OFF_DURATION * 60000UL)) {
        pumpOn = true;
        lastToggle = ms;
        control_pump(true);
        Serial.printf("🔄 Pump cycle ON — idle %lu min, now %02d:%02d\n",
                      elapsedMin, now.tm_hour, now.tm_min);
    }
}


// ===== LIGHT LOGIC =====
// Controls grow lights based only on time schedule (photoperiod).
// Air temperature/humidity are not considered here.
void check_and_control_light(const struct tm& now, const CommandState& cmd) {
    if (handle_manual_mode(cmd, control_light, "💡 Light")) return;

    static bool lightOn = false;
    static unsigned long lastToggle = 0;

    int mins = now.tm_hour * 60 + now.tm_min;

    // Schedule: ON during morning or evening window
    bool shouldBeOn = (mins >= LIGHT_MORNING_ON && mins < LIGHT_MORNING_OFF) ||
                      (mins >= LIGHT_EVENING_ON && mins < LIGHT_EVENING_OFF);

    if (shouldBeOn && !lightOn) {
        lightOn = true;
        lastToggle = millis();
        control_light(true);
        Serial.printf("💡 Light AUTO ON — time %02d:%02d\n", now.tm_hour, now.tm_min);
    } 
    else if (!shouldBeOn && lightOn) {
        unsigned long runtimeMin = (millis() - lastToggle) / 60000UL;
        lightOn = false;
        lastToggle = millis();
        control_light(false);
        Serial.printf("💡 Light AUTO OFF — ran %lu min, now %02d:%02d\n",
                      runtimeMin, now.tm_hour, now.tm_min);
    }
}


// ===== VALVE LOGIC =====
// Opens valve when float indicates low water, closes on high water or timeout.
void check_and_control_valve(bool waterLevelLow, const CommandState& cmd, const struct tm& now) {
    if (handle_manual_mode(cmd, control_valve, "🚰 Valve")) return;

    unsigned long ms = millis();

    // Update debounce timers
    if (waterLevelLow) {
        if (valveState.floatLowSince == 0) valveState.floatLowSince = ms;
        valveState.floatHighSince = 0;
    } else {
        if (valveState.floatHighSince == 0) valveState.floatHighSince = ms;
        valveState.floatLowSince = 0;
    }

    bool debouncedLow  = waterLevelLow && millis_elapsed(ms, valveState.floatLowSince) >= FLOAT_LOW_DEBOUNCE_MS;
    bool debouncedHigh = !waterLevelLow && millis_elapsed(ms, valveState.floatHighSince) >= FLOAT_HIGH_DEBOUNCE_MS;

    // Start refill
    if (debouncedLow && !valveState.open) {
        valveState.open = true;
        valveState.openSince = ms;
        control_valve(true);
        Serial.printf("🚰 Valve AUTO OPEN — low water detected (%02d:%02d)\n", now.tm_hour, now.tm_min);
    }

    // Stop refill
    if (valveState.open) {
        bool timeout = millis_elapsed(ms, valveState.openSince) >= VALVE_MAX_OPEN_MS;
        if (debouncedHigh || timeout) {
            unsigned long runtimeMin = (ms - valveState.openSince) / 60000UL;
            valveState.open = false;
            control_valve(false);
            Serial.printf("🚰 Valve AUTO CLOSED — %s after %lu min (%02d:%02d)\n",
                          timeout ? "timeout" : "water full", runtimeMin, now.tm_hour, now.tm_min);
        }
    }
}

// ===== MAIN DISPATCH =====
void apply_rules(const RealTimeData& current, const struct tm& now, const Commands& commands) {
    int currentMinutes = now.tm_hour * 60 + now.tm_min;
    bool waterLevelLow = current.floatTriggered;

    check_climate_and_control_fan(current.airTemp, current.airHumidity, currentMinutes, commands.fan);
    check_and_control_pump(waterLevelLow, commands.pump, now);
    check_and_control_light(now, commands.light);
    check_and_control_valve(waterLevelLow, commands.valve, now);
}
