#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include "firestore_client.h"
#include <time.h>
#include "time_utils.h"

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
// Time-aware pump control
void check_and_control_pump(bool waterLevelLow, const CommandState& cmd, const struct tm& now) {
    if (!cmd.isAuto) {
        control_pump(cmd.value);
        return;
    }

    static bool pumpOn = false;
    static unsigned long lastToggle = 0;
    unsigned long nowMillis = millis();

    // Safety: Float switch
    if (waterLevelLow) {
        if (pumpOn) {
            control_pump(false);
            pumpOn = false;
        }
        return;
    }

    // First run
    if (lastToggle == 0) {
        pumpOn = true;
        lastToggle = nowMillis;
        control_pump(true);
        return;
    }

    // Scheduled cycle (e.g., 5 min ON/OFF)
    if (pumpOn && nowMillis - lastToggle >= (PUMP_ON_DURATION * 60000UL)) {
        pumpOn = false;
        lastToggle = nowMillis;
        control_pump(false);
    } else if (!pumpOn && nowMillis - lastToggle >= (PUMP_OFF_DURATION * 60000UL)) {
        pumpOn = true;
        lastToggle = nowMillis;
        control_pump(true);
    }
}

// Fallback pump control (millis-based only)
void check_and_control_pump_fallback(bool waterLevelLow, const CommandState& cmd, unsigned long nowMillis) {
    if (!cmd.isAuto) {
        control_pump(cmd.value);
        return;
    }

    static bool pumpOn = false;
    static unsigned long lastToggle = 0;

    // Safety: Float switch
    if (waterLevelLow) {
        if (pumpOn) {
            control_pump(false);
            pumpOn = false;
        }
        return;
    }

    // First run
    if (lastToggle == 0) {
        pumpOn = true;
        lastToggle = nowMillis;
        control_pump(true);
        return;
    }

    // Scheduled cycle (e.g., 5 min ON/OFF)
    if (pumpOn && nowMillis - lastToggle >= (PUMP_ON_DURATION * 60000UL)) {
        pumpOn = false;
        lastToggle = nowMillis;
        control_pump(false);
    } else if (!pumpOn && nowMillis - lastToggle >= (PUMP_OFF_DURATION * 60000UL)) {
        pumpOn = true;
        lastToggle = nowMillis;
        control_pump(true);
    }
}

// Fallback fan control (no time dependency, always active)
void check_climate_and_control_fan_fallback(float airTemp, float humidity, const CommandState& cmd) {
    if (handle_manual_mode(cmd, control_fan, "🌀 Fan")) return;
    if (isnan(airTemp) || isnan(humidity)) return;

    static bool fanActive = false;
    static unsigned long fanOnSince = 0;
    unsigned long now = millis();

    // Thresholds (same as time-aware version)
    bool tempHigh = airTemp >= TEMP_ON_THRESHOLD;
    bool tempLow  = airTemp < TEMP_OFF_THRESHOLD;
    bool humHigh  = humidity > HUMIDITY_ON_THRESHOLD;
    bool humLow   = humidity < HUMIDITY_OFF_THRESHOLD;
    bool emergency = airTemp >= TEMP_EMERGENCY;

    // Decisions (always respond to conditions, no time restrictions)
    bool shouldOn = (!fanActive) && (emergency || tempHigh || humHigh);
    bool shouldOff = fanActive && tempLow && humLow;

    // Safety: max runtime
    if (fanActive && millis_elapsed(now, fanOnSince) >= FAN_MAX_CONTINUOUS_MS) {
        shouldOff = true;
    }

    // Apply state
    if (shouldOn) {
        fanActive = true;
        fanOnSince = now;
        control_fan(true);
        Serial.printf("🌀 Fan FALLBACK ON — T=%.1f°C, H=%.1f%% (No time)\n",
                      airTemp, humidity);
    } 
    else if (shouldOff && millis_elapsed(now, fanOnSince) >= FAN_MINUTE_RUNTIME) {
        fanActive = false;
        control_fan(false);
        Serial.println("✅ Fan FALLBACK OFF — Normalized or safety limit");
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

// Fallback light control (simple on/off based on manual command only)
void check_and_control_light_fallback(const CommandState& cmd) {
    if (handle_manual_mode(cmd, control_light, "💡 Light")) return;
    
    // In fallback mode, lights are OFF by default (safety)
    // Only manual override can turn them on
    static bool lightOn = false;
    if (!lightOn) {
        control_light(false);
        Serial.println("💡 Light FALLBACK OFF — No time schedule available");
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

// Fallback valve control (millis-based, no time logging)
void check_and_control_valve_fallback(bool waterLevelLow, const CommandState& cmd, unsigned long nowMillis) {
    if (handle_manual_mode(cmd, control_valve, "🚰 Valve")) return;

    // Update debounce timers
    if (waterLevelLow) {
        if (valveState.floatLowSince == 0) valveState.floatLowSince = nowMillis;
        valveState.floatHighSince = 0;
    } else {
        if (valveState.floatHighSince == 0) valveState.floatHighSince = nowMillis;
        valveState.floatLowSince = 0;
    }

    bool debouncedLow  = waterLevelLow && millis_elapsed(nowMillis, valveState.floatLowSince) >= FLOAT_LOW_DEBOUNCE_MS;
    bool debouncedHigh = !waterLevelLow && millis_elapsed(nowMillis, valveState.floatHighSince) >= FLOAT_HIGH_DEBOUNCE_MS;

    // Start refill
    if (debouncedLow && !valveState.open) {
        valveState.open = true;
        valveState.openSince = nowMillis;
        control_valve(true);
        Serial.println("🚰 Valve FALLBACK OPEN — low water detected (no time)");
    }

    // Stop refill
    if (valveState.open) {
        bool timeout = millis_elapsed(nowMillis, valveState.openSince) >= VALVE_MAX_OPEN_MS;
        if (debouncedHigh || timeout) {
            unsigned long runtimeMin = (nowMillis - valveState.openSince) / 60000UL;
            valveState.open = false;
            control_valve(false);
            Serial.printf("🚰 Valve FALLBACK CLOSED — %s after %lu min (no time)\n",
                          timeout ? "timeout" : "water full", runtimeMin);
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


// === Unified Actuator Control ===
void updateActuators(const RealTimeData& current, Commands& commands, unsigned long nowMillis) {
    bool waterLevelLow = current.floatTriggered;

    // Emergency fallback
    if (waterLevelLow) {
        control_pump(false);
        if (!valveState.open) {
            valveState.open = true;
            valveState.openSince = nowMillis;
            control_valve(true);
        }
    } else {
        if (valveState.open) {
            valveState.open = false;
            control_valve(false);
        }
    }

    // Check if time is available
    if (isTimeAvailable()) {
        struct tm timeinfo;
        if (getLocalTm(timeinfo)) {
            // Time-aware control for all actuators
            apply_rules(current, timeinfo, commands);
        } else {
            // Time sync failed, use fallback
            Serial.println("⚠️ Time sync failed, using fallback control");
            check_climate_and_control_fan_fallback(current.airTemp, current.airHumidity, commands.fan);
            check_and_control_pump_fallback(waterLevelLow, commands.pump, nowMillis);
            check_and_control_light_fallback(commands.light);
            check_and_control_valve_fallback(waterLevelLow, commands.valve, nowMillis);
        }
    } else {
        // No time available, use fallback control
        Serial.println("⚠️ No time available, using fallback control");
        check_climate_and_control_fan_fallback(current.airTemp, current.airHumidity, commands.fan);
        check_and_control_pump_fallback(waterLevelLow, commands.pump, nowMillis);
        check_and_control_light_fallback(commands.light);
        check_and_control_valve_fallback(waterLevelLow, commands.valve, nowMillis);
    }
}

