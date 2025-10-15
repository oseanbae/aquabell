#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include "firebase.h"
#include <time.h>
#include "time_utils.h"

// Forward declarations
void applyRulesWithModeControl(RealTimeData& data, ActuatorState& actuators, const Commands& commands, unsigned long nowMillis);
void checkValveFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis);
void checkPumpFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis);
void checkFanFallback(ActuatorState& actuators, float airTemp, float humidity, unsigned long nowMillis);
void checkLightFallback(ActuatorState& actuators, unsigned long nowMillis);

// === External globals ===
extern RealTimeData current;
extern ActuatorState actuators;
extern Commands currentCommands;
extern unsigned long lastStreamUpdate;
extern unsigned long lastRelaySync;

// === RULE EVALUATION ENTRY ===
void evaluateRules(bool forceImmediate) {
    unsigned long nowMillis = millis();

    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    bool fbReady = isFirebaseReady();
    bool cmdsSynced = isInitialCommandsSynced();

    if (wifiUp && (!fbReady || !cmdsSynced)) {
        Serial.println("[EVAL] Firebase not ready — holding actuators OFF");
        actuators.fan = false;
        actuators.light = false;
        actuators.pump = false;
        actuators.valve = false;
        return;
    }

    // Apply rules — rule function handles auto flags internally
    applyRulesWithModeControl(current, actuators, currentCommands, nowMillis);

    // Sync and reflect states
    if (wifiUp && fbReady && cmdsSynced) {
        bool debounceOk = (millis() - lastStreamUpdate > 5000) &&
                          (millis() - lastRelaySync > RELAY_SYNC_COOLDOWN);
        if (forceImmediate || debounceOk) {
            syncRelayState(current, currentCommands);
            pushToRTDBLive(current);
            lastRelaySync = millis();
        } else {
            Serial.println("[EVAL] Skipping sync — debounce/cooldown active");
        }
    }
}


// --- MAIN ENTRY ---
void applyRulesWithModeControl(RealTimeData& data, ActuatorState& actuators, const Commands& commands, unsigned long nowMillis) {
    Serial.println("[RULE_ENGINE] Applying mode-aware rules: AUTO=ESP32 control, MANUAL=RTDB control");

    // ===== 1. EMERGENCY OVERRIDES =====
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

        control_fan(actuators.fan);
        control_light(actuators.light);
        control_pump(actuators.pump);
        control_valve(actuators.valve);
        return;
    }

    // ===== 2. AUTO/MANUAL MODE PROCESSING =====
    static bool prevFanAuto   = true;
    static bool prevLightAuto = true;
    static bool prevPumpAuto  = true;
    static bool prevValveAuto = true;

    bool fanAutoTransition   = commands.fan.isAuto   && !prevFanAuto;
    bool lightAutoTransition = commands.light.isAuto && !prevLightAuto;
    bool pumpAutoTransition  = commands.pump.isAuto  && !prevPumpAuto;
    bool valveAutoTransition = commands.valve.isAuto && !prevValveAuto;

    // --- FAN ---
    if (!commands.fan.isAuto) {
        actuators.fanAuto = false;
        actuators.fan = commands.fan.value;
        Serial.printf("[RULE_ENGINE] 🌀 Fan MANUAL from RTDB: %s\n", actuators.fan ? "ON" : "OFF");
    } else {
        actuators.fanAuto = true;
        if (fanAutoTransition) {
            actuators.fanAutoJustEnabled = true;
            Serial.println("[RULE_ENGINE] 🌀 Fan AUTO re-enabled — evaluating now");
        }
    }

    // --- LIGHT ---
    if (!commands.light.isAuto) {
        actuators.lightAuto = false;
        actuators.light = commands.light.value;
        Serial.printf("[RULE_ENGINE] 💡 Light MANUAL from RTDB: %s\n", actuators.light ? "ON" : "OFF");
    } else {
        actuators.lightAuto = true;
        if (lightAutoTransition) {
            actuators.lightAutoJustEnabled = true;
            Serial.println("[RULE_ENGINE] 💡 Light AUTO re-enabled — evaluating now");
        }
    }

    // --- PUMP ---
    if (!commands.pump.isAuto) {
        actuators.pumpAuto = false;
        actuators.pump = commands.pump.value;
        Serial.printf("[RULE_ENGINE] 💧 Pump MANUAL from RTDB: %s\n", actuators.pump ? "ON" : "OFF");
    } else {
        actuators.pumpAuto = true;
        if (pumpAutoTransition) {
            actuators.pumpAutoJustEnabled = true;
            Serial.println("[RULE_ENGINE] 💧 Pump AUTO re-enabled — evaluating now");
        }
    }

    // --- VALVE ---
    if (!commands.valve.isAuto) {
        actuators.valveAuto = false;
        actuators.valve = commands.valve.value;
        Serial.printf("[RULE_ENGINE] 🚰 Valve MANUAL from RTDB: %s\n", actuators.valve ? "ON" : "OFF");
    } else {
        actuators.valveAuto = true;
        if (valveAutoTransition) {
            actuators.valveAutoJustEnabled = true;
            Serial.println("[RULE_ENGINE] 🚰 Valve AUTO re-enabled — evaluating now");
        }
    }

    // ===== 3. AUTO CONTROL RULES =====
    if (actuators.fanAuto)   checkFanFallback(actuators, data.airTemp, data.airHumidity, nowMillis);
    if (actuators.lightAuto) checkLightFallback(actuators, nowMillis);
    if (actuators.pumpAuto)  checkPumpFallback(actuators, data.floatTriggered, nowMillis);
    if (actuators.valveAuto) checkValveFallback(actuators, data.floatTriggered, nowMillis);

    // ===== 4. APPLY FINAL STATES =====
    control_fan(actuators.fan);
    control_light(actuators.light);
    control_pump(actuators.pump);
    control_valve(actuators.valve);

    // reflect to global current so syncRelayState uses the actual actuators state
    extern RealTimeData current;
    current.relayStates.fan       = actuators.fan;
    current.relayStates.light     = actuators.light;
    current.relayStates.waterPump = actuators.pump;
    current.relayStates.valve     = actuators.valve;


    Serial.printf("[RULE_ENGINE] Final states - Fan:%s(%s) Light:%s(%s) Pump:%s(%s) Valve:%s(%s)\n",
                  actuators.fan ? "ON" : "OFF", actuators.fanAuto ? "AUTO" : "MANUAL",
                  actuators.light ? "ON" : "OFF", actuators.lightAuto ? "AUTO" : "MANUAL",
                  actuators.pump ? "ON" : "OFF", actuators.pumpAuto ? "AUTO" : "MANUAL",
                  actuators.valve ? "ON" : "OFF", actuators.valveAuto ? "AUTO" : "MANUAL");

    // // Reset re-enable flags after evaluation
    // actuators.fanAutoJustEnabled   = false;
    // actuators.lightAutoJustEnabled = false;
    // actuators.pumpAutoJustEnabled  = false;
    // actuators.valveAutoJustEnabled = false;

    // Update previous state trackers
    prevFanAuto   = commands.fan.isAuto;
    prevLightAuto = commands.light.isAuto;
    prevPumpAuto  = commands.pump.isAuto;
    prevValveAuto = commands.valve.isAuto;
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

// ===== FALLBACK CONTROL FUNCTIONS =====
void checkValveFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis) {
    static unsigned long floatLowSince  = 0;
    static unsigned long floatHighSince = 0;
    static unsigned long valveOpenSince = 0;
    static bool prevValve = false;

    // Reset timers when auto mode re-enabled
    if (actuators.valveAutoJustEnabled) {
        floatLowSince = 0;
        floatHighSince = 0;
        valveOpenSince = 0;
        actuators.valveAutoJustEnabled = false;
    }

    // Track float changes
    if (waterLevelLow) {
        if (floatLowSince == 0) floatLowSince = nowMillis;
        floatHighSince = 0;
    } else {
        if (floatHighSince == 0) floatHighSince = nowMillis;
        floatLowSince = 0;
    }

    bool lowStable  = waterLevelLow  && (nowMillis - floatLowSince  >= FLOAT_LOW_DEBOUNCE_MS);
    bool highStable = !waterLevelLow && (nowMillis - floatHighSince >= FLOAT_HIGH_DEBOUNCE_MS);

    // Trigger valve ON when low level is stable
    if (lowStable && !actuators.valve) {
        actuators.valve = true;
        valveOpenSince = nowMillis;
        Serial.println("[RULE_ENGINE] 🚰 Valve AUTO OPEN — low water detected");
    }

    // Trigger valve OFF when high level stable or timeout reached
    if (actuators.valve) {
        bool timeout = (nowMillis - valveOpenSince >= VALVE_MAX_OPEN_MS);
        if (highStable || timeout) {
            actuators.valve = false;
            valveOpenSince = 0;
            floatLowSince = 0;
            Serial.printf("[RULE_ENGINE] 🚰 Valve AUTO CLOSED — %s\n", timeout ? "timeout" : "water full");
        }
    }

    // Apply hardware state change
    if (actuators.valve != prevValve) {
        control_valve(actuators.valve);
        prevValve = actuators.valve;
    }
}

void checkPumpFallback(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis) {
    static unsigned long lastToggle = 0;
    static bool pumpWasOn = false;
    static bool firstRun = true;

    const unsigned long ON_DURATION  = PUMP_ON_DURATION  * 60 * 1000UL;
    const unsigned long OFF_DURATION = PUMP_OFF_DURATION * 60 * 1000UL;
    //const unsigned long GRACE_PERIOD_MS = 5000; // 5 sec grace after AUTO re-enable

    // Initialize baseline
    if (firstRun) {
        lastToggle = nowMillis - OFF_DURATION; // start as if just completed an off cycle
        firstRun = false;
    }

    // --- When AUTO is re-enabled ---
    if (actuators.pumpAutoJustEnabled) {
        actuators.pumpAutoJustEnabled = false;

        unsigned long elapsed = nowMillis - lastToggle;
        bool inOnPhase = pumpWasOn && elapsed < ON_DURATION;
        bool inOffPhase = !pumpWasOn && elapsed < OFF_DURATION;

        if (inOnPhase) {
            actuators.pump = true;
            Serial.printf("[RULE_ENGINE] 💧 Pump AUTO re-enabled — resuming ON phase (%.1f min left)\n",
                          (ON_DURATION - elapsed) / 60000.0);
        } else if (inOffPhase) {
            actuators.pump = false;
            Serial.printf("[RULE_ENGINE] 💧 Pump AUTO re-enabled — resuming OFF phase (%.1f min left)\n",
                          (OFF_DURATION - elapsed) / 60000.0);
        } else {
            // cycle complete → toggle
            actuators.pump = !pumpWasOn;
            lastToggle = nowMillis;
            Serial.println("[RULE_ENGINE] 💧 Pump AUTO re-enabled — cycle expired, toggling phase");
        }

        pumpWasOn = actuators.pump;
        control_pump(actuators.pump);
        return;
    }

    // --- Normal rule logic below ---
    if (waterLevelLow) {
        if (actuators.pump) {
            actuators.pump = false;
            lastToggle = nowMillis;
            pumpWasOn = false;
            Serial.println("[RULE_ENGINE] 💧 Pump OFF — float switch LOW");
            control_pump(false);
        }
        return;
    }

    unsigned long elapsed = nowMillis - lastToggle;
    if (pumpWasOn && elapsed >= ON_DURATION) {
        actuators.pump = false;
        lastToggle = nowMillis;
        pumpWasOn = false;
        Serial.println("[RULE_ENGINE] 💧 Pump OFF — schedule");
        control_pump(false);
    } else if (!pumpWasOn && elapsed >= OFF_DURATION) {
        actuators.pump = true;
        lastToggle = nowMillis;
        pumpWasOn = true;
        Serial.println("[RULE_ENGINE] 💧 Pump ON — schedule");
        control_pump(true);
    }
}

void checkFanFallback(ActuatorState& actuators, float airTemp, float humidity, unsigned long nowMillis) {
    if (isnan(airTemp) || isnan(humidity)) return;

    static unsigned long fanOnSince = 0;
    static unsigned long fanOffSince = 0;
    static bool prevFan = false;

    // Reset tracking if auto mode just got re-enabled
    if (actuators.fanAutoJustEnabled) {
        fanOnSince = 0;
        fanOffSince = 0;
        actuators.fanAutoJustEnabled = false;
    }

    // Thresholds
    const bool tempEmergency = airTemp >= TEMP_EMERGENCY;
    const bool tempHigh      = airTemp >= TEMP_ON_THRESHOLD;
    const bool tempLow       = airTemp <  TEMP_OFF_THRESHOLD;
    const bool humHigh       = humidity >= HUMIDITY_ON_THRESHOLD;

    // Determine desired action
    bool shouldTurnOn  = (!actuators.fan) && (tempEmergency || tempHigh || humHigh);
    bool shouldTurnOff = (actuators.fan && tempLow);

    // Max runtime safety (fan off after continuous run)
    if (actuators.fan && (nowMillis - fanOnSince >= FAN_MAX_CONTINUOUS_MS)) {
        shouldTurnOff = true;
        Serial.println("[RULE_ENGINE] 🌀 Fan AUTO OFF — exceeded max runtime");
    }

    // Apply decisions with debouncing
    if (shouldTurnOn && (nowMillis - fanOffSince >= FAN_MIN_COOLDOWN_MS)) {
        actuators.fan = true;
        fanOnSince = nowMillis;
        Serial.printf("[RULE_ENGINE] 🌀 Fan AUTO ON — T=%.1f°C, H=%.1f%%\n", airTemp, humidity);
    }
    else if (shouldTurnOff && (nowMillis - fanOnSince >= FAN_MINUTE_RUNTIME)) {
        actuators.fan = false;
        fanOffSince = nowMillis;
        Serial.println("[RULE_ENGINE] ✅ Fan AUTO OFF — normalized or cooldown active");
    }

    // Apply hardware state only if changed
    if (actuators.fan != prevFan) {
        control_fan(actuators.fan);
        prevFan = actuators.fan;
    }
}


void checkLightFallback(ActuatorState& actuators, unsigned long nowMillis) {
    static bool prevLight = false;

    if (actuators.lightAutoJustEnabled) actuators.lightAutoJustEnabled = false;

    if (isTimeAvailable()) {
        struct tm timeinfo;
        if (getLocalTm(timeinfo)) {
            int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            bool shouldBeOn =
                (currentMinutes >= LIGHT_MORNING_ON  && currentMinutes < LIGHT_MORNING_OFF) ||
                (currentMinutes >= LIGHT_EVENING_ON && currentMinutes < LIGHT_EVENING_OFF);

            if (shouldBeOn && !actuators.light) {
                actuators.light = true;
                Serial.printf("[RULE_ENGINE] 💡 Light AUTO ON — %02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
            } else if (!shouldBeOn && actuators.light) {
                actuators.light = false;
                Serial.printf("[RULE_ENGINE] 💡 Light AUTO OFF — %02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
            }
        }
    } else if (actuators.light) {
        actuators.light = false;
        Serial.println("[RULE_ENGINE] 💡 Light AUTO OFF — No time available");
    }

    if (actuators.light != prevLight) {
        control_light(actuators.light);
        prevLight = actuators.light;
    }
}