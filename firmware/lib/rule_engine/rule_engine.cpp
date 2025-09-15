#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include "firestore_client.h"
#include <time.h>
#include "time_utils.h"

// ===== CLEAN RULE ENGINE =====

// Forward declarations for fallback functions
void checkValveFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis);
void checkPumpFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis);
void checkFanFallback(ActuatorState& actuators, float airTemp, float humidity, unsigned long nowMillis);
void checkLightFallback(ActuatorState& actuators, unsigned long nowMillis);

// New mode-aware rule engine that handles AUTO/MANUAL mode control
void applyRulesWithModeControl(RealTimeData& data, ActuatorState& actuators, const Commands& commands, unsigned long nowMillis) {
    Serial.println("[RULE_ENGINE] Applying mode-aware rules: AUTO=ESP32 control, MANUAL=RTDB control");
    
    // 1. EMERGENCY OVERRIDES (highest priority - always applied regardless of mode)
    actuators.emergencyMode = false;
    actuators.lowWaterEmergency = data.floatTriggered;
    actuators.highTempEmergency = !isnan(data.airTemp) && data.airTemp >= TEMP_EMERGENCY;
    
    if (actuators.lowWaterEmergency || actuators.highTempEmergency) {
        actuators.emergencyMode = true;
        Serial.println("[RULE_ENGINE] 🚨 EMERGENCY MODE ACTIVATED");
        
        if (actuators.lowWaterEmergency) {
            Serial.println("[RULE_ENGINE] 💧 Low water emergency - shutting off pump, opening valve");
            actuators.pump = false;
            actuators.valve = true;
        }
        
        if (actuators.highTempEmergency) {
            Serial.println("[RULE_ENGINE] 🌡️ High temperature emergency - turning on fan");
            actuators.fan = true;
        }
        
        // Emergency overrides all other controls
        control_fan(actuators.fan);
        control_light(actuators.light);
        control_pump(actuators.pump);
        control_valve(actuators.valve);
        return;
    }
    
    // 2. MANUAL MODE: Use values from RTDB (app/user control)
    if (!commands.fan.isAuto) {
        actuators.fan = commands.fan.value;
        actuators.fanAuto = false;
        Serial.printf("[RULE_ENGINE] 🌀 Fan MANUAL from RTDB: %s\n", actuators.fan ? "ON" : "OFF");
    } else {
        actuators.fanAuto = true;
    }
    
    if (!commands.light.isAuto) {
        actuators.light = commands.light.value;
        actuators.lightAuto = false;
        Serial.printf("[RULE_ENGINE] 💡 Light MANUAL from RTDB: %s\n", actuators.light ? "ON" : "OFF");
    } else {
        actuators.lightAuto = true;
    }
    
    if (!commands.pump.isAuto) {
        actuators.pump = commands.pump.value;
        actuators.pumpAuto = false;
        Serial.printf("[RULE_ENGINE] 💧 Pump MANUAL from RTDB: %s\n", actuators.pump ? "ON" : "OFF");
    } else {
        actuators.pumpAuto = true;
    }
    
    if (!commands.valve.isAuto) {
        actuators.valve = commands.valve.value;
        actuators.valveAuto = false;
        Serial.printf("[RULE_ENGINE] 🚰 Valve MANUAL from RTDB: %s\n", actuators.valve ? "ON" : "OFF");
    } else {
        actuators.valveAuto = true;
    }
    
    // 3. AUTO MODE: ESP32 controls based on sensor data and rules
    if (actuators.fanAuto) {
        checkFanFallback(actuators, data.airTemp, data.airHumidity, nowMillis);
    }
    
    if (actuators.lightAuto) {
        checkLightFallback(actuators, nowMillis);
    }
    
    if (actuators.pumpAuto) {
        checkPumpFallback(actuators, data.floatTriggered, nowMillis);
    }
    
    if (actuators.valveAuto) {
        checkValveFallback(actuators, data.floatTriggered, nowMillis);
    }
    
    // 4. Apply final states to physical relays (always apply correct values)
    control_fan(actuators.fan);
    control_light(actuators.light);
    control_pump(actuators.pump);
    control_valve(actuators.valve);
    
    Serial.printf("[RULE_ENGINE] Final states - Fan:%s(%s) Light:%s(%s) Pump:%s(%s) Valve:%s(%s)\n",
                 actuators.fan ? "ON" : "OFF", actuators.fanAuto ? "AUTO" : "MANUAL",
                 actuators.light ? "ON" : "OFF", actuators.lightAuto ? "AUTO" : "MANUAL",
                 actuators.pump ? "ON" : "OFF", actuators.pumpAuto ? "AUTO" : "MANUAL",
                 actuators.valve ? "ON" : "OFF", actuators.valveAuto ? "AUTO" : "MANUAL");
}

// Legacy function for backward compatibility (simplified)
void applyRules(RealTimeData& data, ActuatorState& actuators, unsigned long nowMillis) {
    // Create default commands (all AUTO mode)
    Commands defaultCommands = {
        {true, false}, // fan
        {true, false}, // light
        {true, false}, // pump
        {true, false}  // valve
    };
    
    // Use the new mode-aware function
    applyRulesWithModeControl(data, actuators, defaultCommands, nowMillis);
}

// ===== FALLBACK CONTROL FUNCTIONS =====

void checkValveFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis) {
    static unsigned long floatLowSince = 0;
    static unsigned long floatHighSince = 0;
    static unsigned long valveOpenSince = 0;
    
    // Update debounce timers
    if (waterLevelLow) {
        if (floatLowSince == 0) floatLowSince = nowMillis;
        floatHighSince = 0;
    } else {
        if (floatHighSince == 0) floatHighSince = nowMillis;
        floatLowSince = 0;
    }
    
    bool debouncedLow = waterLevelLow && (nowMillis - floatLowSince >= FLOAT_LOW_DEBOUNCE_MS);
    bool debouncedHigh = !waterLevelLow && (nowMillis - floatHighSince >= FLOAT_HIGH_DEBOUNCE_MS);
    
    // Start refill
    if (debouncedLow && !actuators.valve) {
        actuators.valve = true;
        valveOpenSince = nowMillis;
        Serial.println("[RULE_ENGINE] 🚰 Valve AUTO OPEN — low water detected");
    }
    
    // Stop refill
    if (actuators.valve) {
        bool timeout = (nowMillis - valveOpenSince >= VALVE_MAX_OPEN_MS);
        if (debouncedHigh || timeout) {
            actuators.valve = false;
            valveOpenSince = 0;
            Serial.printf("[RULE_ENGINE] 🚰 Valve AUTO CLOSED — %s\n",
                         timeout ? "timeout" : "water full");
        }
    }
}

void checkPumpFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis) {
    static unsigned long lastToggle = 0;
    
    // Safety: turn OFF if float switch is low
    if (waterLevelLow) {
        if (actuators.pump) {
            actuators.pump = false;
            Serial.println("[RULE_ENGINE] 💧 Pump OFF — float switch LOW");
        }
        return;
    }
    
    // First run: start the ON cycle immediately
    if (lastToggle == 0) {
        actuators.pump = true;
        lastToggle = nowMillis;
        Serial.println("[RULE_ENGINE] 💧 Pump ON — first run");
        return;
    }
    
    // Scheduled cycle: ON/OFF durations
    const unsigned long ON_DURATION = PUMP_ON_DURATION * 60 * 1000UL; // Convert minutes to ms
    const unsigned long OFF_DURATION = PUMP_OFF_DURATION * 60 * 1000UL;
    
    if (actuators.pump && (nowMillis - lastToggle >= ON_DURATION)) {
        actuators.pump = false;
        lastToggle = nowMillis;
        Serial.println("[RULE_ENGINE] 💧 Pump OFF — schedule");
    } else if (!actuators.pump && (nowMillis - lastToggle >= OFF_DURATION)) {
        actuators.pump = true;
        lastToggle = nowMillis;
        Serial.println("[RULE_ENGINE] 💧 Pump ON — schedule");
    }
}

void checkFanFallback(ActuatorState& actuators, float airTemp, float humidity, unsigned long nowMillis) {
    if (isnan(airTemp) || isnan(humidity)) return;
    
    static unsigned long fanOnSince = 0;
    
    // Thresholds
    bool tempHigh = airTemp >= TEMP_ON_THRESHOLD;
    bool tempLow = airTemp < TEMP_OFF_THRESHOLD;
    bool humHigh = humidity > HUMIDITY_ON_THRESHOLD;
    bool humLow = humidity < HUMIDITY_OFF_THRESHOLD;
    bool emergency = airTemp >= TEMP_EMERGENCY;
    
    // Decisions
    bool shouldOn = (!actuators.fan) && (emergency || tempHigh || humHigh);
    bool shouldOff = actuators.fan && tempLow && humLow;
    
    // Safety: max runtime
    if (actuators.fan && (nowMillis - fanOnSince >= FAN_MAX_CONTINUOUS_MS)) {
        shouldOff = true;
    }
    
    // Apply state
    if (shouldOn) {
        actuators.fan = true;
        fanOnSince = nowMillis;
        Serial.printf("[RULE_ENGINE] 🌀 Fan AUTO ON — T=%.1f°C, H=%.1f%%\n",
                     airTemp, humidity);
    } else if (shouldOff && (nowMillis - fanOnSince >= FAN_MINUTE_RUNTIME)) {
        actuators.fan = false;
        Serial.println("[RULE_ENGINE] ✅ Fan AUTO OFF — Normalized or safety limit");
    }
}

void checkLightFallback(ActuatorState& actuators, unsigned long nowMillis) {
    // Check if time is available for schedule-based control
    if (isTimeAvailable()) {
        struct tm timeinfo;
        if (getLocalTm(timeinfo)) {
            int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            
            // Schedule: ON during morning or evening window
            bool shouldBeOn = (currentMinutes >= LIGHT_MORNING_ON && currentMinutes < LIGHT_MORNING_OFF) ||
                            (currentMinutes >= LIGHT_EVENING_ON && currentMinutes < LIGHT_EVENING_OFF);
            
            if (shouldBeOn && !actuators.light) {
                actuators.light = true;
                Serial.printf("[RULE_ENGINE] 💡 Light AUTO ON — time %02d:%02d\n", 
                             timeinfo.tm_hour, timeinfo.tm_min);
            } else if (!shouldBeOn && actuators.light) {
                actuators.light = false;
                Serial.printf("[RULE_ENGINE] 💡 Light AUTO OFF — time %02d:%02d\n", 
                             timeinfo.tm_hour, timeinfo.tm_min);
            }
            return;
        }
    }
    
    // Fallback: lights OFF by default (safety)
    if (actuators.light) {
        actuators.light = false;
        Serial.println("[RULE_ENGINE] 💡 Light AUTO OFF — No time schedule available");
    }
}

// ===== LEGACY FUNCTIONS (for backward compatibility) =====

void updateActuators(RealTimeData& current, Commands& commands, unsigned long nowMillis) {
    // Create ActuatorState from current data
    ActuatorState actuators = {};
    actuators.fan = current.relayStates.fan;
    actuators.light = current.relayStates.light;
    actuators.pump = current.relayStates.waterPump;
    actuators.valve = current.relayStates.valve;
    
    // Use the new mode-aware function
    applyRulesWithModeControl(current, actuators, commands, nowMillis);
}

void apply_rules(RealTimeData& current, const struct tm& now, const Commands& commands) {
    // Create ActuatorState from current data
    ActuatorState actuators = {};
    actuators.fan = current.relayStates.fan;
    actuators.light = current.relayStates.light;
    actuators.pump = current.relayStates.waterPump;
    actuators.valve = current.relayStates.valve;
    
    // Use the new mode-aware function
    applyRulesWithModeControl(current, actuators, commands, millis());
}