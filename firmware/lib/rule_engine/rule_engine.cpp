#include <Arduino.h>
#include "config.h"
#include "rule_engine.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include "firebase.h"
#include <time.h>
#include "time_utils.h"

// === External globals ===
extern RealTimeData current;
extern ActuatorState actuators;
extern Commands currentCommands;
extern unsigned long lastStreamUpdate;
extern unsigned long lastRelaySync;

// === RULE EVALUATION ENTRY ===
void evaluateRules(bool forceImmediate) {
    unsigned long nowMillis = millis();

    // Note: pH pump logic runs independently in main loop - no Firebase dependency
    // It's called every loop iteration for proper non-blocking timing control
    
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    bool fbReady = isFirebaseReady();
    bool cmdsSynced = isInitialCommandsSynced();

    // Allow local automation even if Firebase is not ready
    if (!wifiUp || !fbReady || !cmdsSynced) {
        Serial.println("[EVAL] Firebase/WiFi not ready — running LOCAL automation only");
        applyRulesWithModeControl(current, actuators, currentCommands, nowMillis);
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
            // Use empty array if no sensors updated (for relay state sync only)
            bool emptySensors[6] = {false, false, false, false, false, false};
            pushToRTDBLive(current, emptySensors);
            lastRelaySync = millis();
        } else {
            Serial.println("[EVAL] Skipping sync — debounce/cooldown active");
        }
    }
}


// --- MAIN ENTRY ---
void applyRulesWithModeControl(RealTimeData& data, ActuatorState& actuators, const Commands& commands, unsigned long nowMillis) {
    Serial.println("[RULE_ENGINE] Applying mode-aware rules");

    // If emergency active, skip normal rules (emergency already applied)
    if (actuators.emergencyMode) {
        Serial.println("[RULE_ENGINE] 🔒 Emergency active — skipping AUTO/MANUAL logic");
        return;
    }

    // ===== 2. AUTO/MANUAL MODE PROCESSING =====
    static bool prevFanAuto   = true;
    static bool prevLightAuto = true;
    static bool prevPumpAuto  = true;
    static bool prevValveAuto = true;
    static bool prevCoolerAuto = true;
    static bool prevHeaterAuto = true;

    bool fanAutoTransition   = commands.fan.isAuto   && !prevFanAuto;
    bool lightAutoTransition = commands.light.isAuto && !prevLightAuto;
    bool pumpAutoTransition  = commands.pump.isAuto  && !prevPumpAuto;
    bool valveAutoTransition = commands.valve.isAuto && !prevValveAuto;
    bool coolerAutoTransition = commands.cooler.isAuto && !prevCoolerAuto;
    bool heaterAutoTransition = commands.heater.isAuto && !prevHeaterAuto;

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

    // --- WATER COOLER ---
    if (!commands.cooler.isAuto) {
        actuators.coolerAuto = false;
        actuators.cooler = commands.cooler.value;
        Serial.printf("[RULE_ENGINE] 💨 Water Cooler MANUAL from RTDB: %s\n", actuators.cooler ? "ON" : "OFF");
    } else {
        actuators.coolerAuto = true;
        if (coolerAutoTransition) {
            actuators.coolerAutoJustEnabled = true;
            Serial.println("[RULE_ENGINE] 💨 Water Cooler AUTO re-enabled — evaluating now");
        }
    }

    // --- WATER HEATER ---
    if (!commands.heater.isAuto) {
        actuators.heaterAuto = false;
        actuators.heater = commands.heater.value;
        Serial.printf("[RULE_ENGINE] 🔥 Water Heater MANUAL from RTDB: %s\n", actuators.heater ? "ON" : "OFF");
    } else {
        actuators.heaterAuto = true;
        if (heaterAutoTransition) {
            actuators.heaterAutoJustEnabled = true;
            Serial.println("[RULE_ENGINE] 🔥 Water Heater AUTO re-enabled — evaluating now");
        }
    }

    // ===== 3. AUTO CONTROL RULES =====
    if (actuators.fanAuto)   checkFanLogic(actuators, data.airTemp, data.airHumidity, nowMillis);
    if (actuators.lightAuto) checkLightLogic(actuators, nowMillis);
    if (actuators.pumpAuto)  checkPumpLogic(actuators, nowMillis);
    if (actuators.valveAuto) checkValveLogic(actuators, data.floatTriggered, nowMillis);
    if (actuators.coolerAuto) checkCoolerLogic(actuators, data.waterTemp, nowMillis);
    if (actuators.heaterAuto) checkHeaterLogic(actuators, data.waterTemp, nowMillis);

    // ===== 4. APPLY FINAL STATES =====
    control_fan(actuators.fan);
    control_light(actuators.light);
    control_pump(actuators.pump);
    control_valve(actuators.valve);
    control_cooler(actuators.cooler);
    control_heater(actuators.heater);

    // reflect to global current so syncRelayState uses the actual actuators state
    extern RealTimeData current;
    current.relayStates.fan       = actuators.fan;
    current.relayStates.light     = actuators.light;
    current.relayStates.waterPump = actuators.pump;
    current.relayStates.valve     = actuators.valve;
    current.relayStates.cooler = actuators.cooler;
    current.relayStates.heater = actuators.heater;
    // Note: pH pump states are reflected in evaluateRules() since it runs independently

    Serial.printf("[RULE_ENGINE] Final states - Fan:%s(%s) Light:%s(%s) Pump:%s(%s) Valve:%s(%s) Cooler:%s(%s)\n Heater:%s(%s)\n",
                  actuators.fan ? "ON" : "OFF", actuators.fanAuto ? "AUTO" : "MANUAL",
                  actuators.light ? "ON" : "OFF", actuators.lightAuto ? "AUTO" : "MANUAL",
                  actuators.pump ? "ON" : "OFF", actuators.pumpAuto ? "AUTO" : "MANUAL",
                  actuators.valve ? "ON" : "OFF", actuators.valveAuto ? "AUTO" : "MANUAL",
                  actuators.cooler ? "ON" : "OFF", actuators.coolerAuto ? "AUTO" : "MANUAL",
                  actuators.heater ? "ON" : "OFF", actuators.heaterAuto ? "AUTO" : "MANUAL");

    // // Reset re-enable flags after evaluation
    // actuators.fanAutoJustEnabled   = false;
    // actuators.lightAutoJustEnabled = false;
    // actuators.pumpAutoJustEnabled  = false;
    // actuators.valveAutoJustEnabled = false;
    // actuators.coolerAutoJustEnabled = false;
    // actuators.heaterAutoJustEnabled = false;

    // Update previous state trackers
    prevFanAuto   = commands.fan.isAuto;
    prevLightAuto = commands.light.isAuto;
    prevPumpAuto  = commands.pump.isAuto;
    prevValveAuto = commands.valve.isAuto;
    prevCoolerAuto = commands.cooler.isAuto;
    prevHeaterAuto = commands.heater.isAuto;
}

// ===== FALLBACK CONTROL FUNCTIONS =====
void checkValveLogic(ActuatorState& actuators, bool waterLevelLow, unsigned long nowMillis) {
    static unsigned long floatLowSince  = 0;
    static unsigned long floatHighSince = 0;
    static bool intentPrinted = false;
    static bool prevValveState = false;

    // Reset timers when auto mode re-enabled
    if (actuators.valveAutoJustEnabled) {
        floatLowSince = 0;
        floatHighSince = 0;
        intentPrinted = false;
        actuators.valveAutoJustEnabled = false;
    }

    // Track float transitions
    if (waterLevelLow) {
        if (floatLowSince == 0) floatLowSince = nowMillis;
        floatHighSince = 0;
        if (!intentPrinted && !actuators.valve) {
            Serial.println("[RULE_ENGINE] [INTENT] Float LOW detected — valve about to OPEN");
            intentPrinted = true;
        }
    } else {
        if (floatHighSince == 0) floatHighSince = nowMillis;
        floatLowSince = 0;
        if (!intentPrinted && actuators.valve) {
            Serial.println("[RULE_ENGINE] [INTENT] Float HIGH detected — valve about to CLOSE");
            intentPrinted = true;
        }
    }

    bool lowStable  = waterLevelLow  && (nowMillis - floatLowSince  >= FLOAT_LOW_DEBOUNCE_MS); //3000;
    bool highStable = !waterLevelLow && (nowMillis - floatHighSince >= FLOAT_HIGH_DEBOUNCE_MS); //3000;

    // Stable LOW → valve ON
    if (lowStable && !actuators.valve) {
        actuators.valve = true;
        intentPrinted = false; // reset after applying action
        Serial.println("[RULE_ENGINE] 🚰 Valve AUTO OPEN — low water confirmed");
    }

    // Stable HIGH → valve OFF
    if (actuators.valve && highStable) {
        actuators.valve = false;
        intentPrinted = false; // reset after applying action
        floatLowSince = 0;
        Serial.println("[RULE_ENGINE] ✅ Valve AUTO CLOSED — water level normal");
    }

    // Apply relay control if changed
    if (actuators.valve != prevValveState) {
        control_valve(actuators.valve);
        prevValveState = actuators.valve;
    }
}

void checkPumpLogic(ActuatorState& actuators, unsigned long nowMillis) {
    static unsigned long lastToggle = 0;
    static bool pumpWasOn = false;
    static bool firstRun = true;

    const unsigned long ON_DURATION  = PUMP_ON_DURATION  * 60 * 1000UL;
    const unsigned long OFF_DURATION = PUMP_OFF_DURATION * 60 * 1000UL;

    if (firstRun) {
        lastToggle = nowMillis - OFF_DURATION;
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
        }

        pumpWasOn = actuators.pump;
        control_pump(actuators.pump);
        return;
    }

    // --- Pause pump if valve is open ---
    if (actuators.valve) {
        if (actuators.pump) {
            actuators.pump = false;
            Serial.println("[RULE_ENGINE] 💧 Pump OFF — valve open, refilling water");
            control_pump(false);
        }
        return; // skip normal schedule while refilling
    }

    // --- Normal pump schedule ---
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

void checkFanLogic(ActuatorState& actuators, float t, float h, unsigned long now) {
    if (isnan(t) || isnan(h)) return;

    static unsigned long last = 0;
    const unsigned long DEBOUNCE = 5000;

    // Auto re-enable sync
    if (actuators.fanAutoJustEnabled) {
        actuators.fanAutoJustEnabled = false;
        actuators.fan = (t >= TEMP_ON_THRESHOLD);
        control_fan(actuators.fan);
        last = now;
        Serial.printf("[RULE_ENGINE] Fan AUTO sync → %s (T=%.1f, H=%.1f)\n",
                      actuators.fan ? "ON" : "OFF", t, h);
        return;
    }

    if (!actuators.fanAuto) return;

    bool tooHot = t >= TEMP_ON_THRESHOLD;          // 29C
    bool coolEnough = t <= TEMP_OFF_THRESHOLD;     // 26C
    bool tooHumid = h >= HUMIDITY_MAX_THRESHOLD;   // 90%
    bool dryEnough = h <= HUMIDITY_MIN_THRESHOLD;  // 80%

    bool turnOn = tooHot || tooHumid;
    bool turnOff = coolEnough && dryEnough;

    if (turnOn && !actuators.fan && now - last > DEBOUNCE) {
        actuators.fan = true;
        control_fan(true);
        last = now;
        Serial.printf("[RULE_ENGINE] Fan ON (T=%.1f, H=%.1f)\n", t, h);
    }
    else if (turnOff && actuators.fan && now - last > DEBOUNCE) {
        actuators.fan = false;
        control_fan(false);
        last = now;
        Serial.printf("[RULE_ENGINE] Fan OFF (T=%.1f, H=%.1f)\n", t, h);
    }
}

void checkLightLogic(ActuatorState& actuators, unsigned long nowMillis) {
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

void checkCoolerLogic(ActuatorState &actuators, float waterTemp, unsigned long nowMillis)
{
    if (isnan(waterTemp)) return;
    if (!actuators.coolerAuto) return;

    static unsigned long last = 0;
    const unsigned long DEBOUNCE_MS = 5000;


    // Handle AUTO re-enable
    if (actuators.coolerAutoJustEnabled) {
        actuators.coolerAutoJustEnabled = false;

        if (waterTemp >= COOLER_ON_TEMP) {
            actuators.cooler = true;
        } else if (waterTemp <= COOLER_OFF_TEMP) {
            actuators.cooler = false;
        }

        control_cooler(actuators.cooler);
        last = nowMillis;

        Serial.printf("[RULE_ENGINE] 💨 Fan AUTO re-enabled → %s (waterTemp=%.2f°C)\n",
                      actuators.cooler ? "ON" : "OFF", waterTemp);
        return;
    }

    bool tooHot     = (waterTemp >= COOLER_ON_TEMP);
    bool coolEnough = (waterTemp <= COOLER_OFF_TEMP);

    if (tooHot && !actuators.cooler && (nowMillis - last) > DEBOUNCE_MS) {
        actuators.cooler = true;
        control_cooler(true);
        last = nowMillis;
        Serial.printf("[RULE_ENGINE] 💨 Fan ON — water warm (%.2f°C)\n", waterTemp);
    }
    else if (coolEnough && actuators.cooler && (nowMillis - last) > DEBOUNCE_MS) {
        actuators.cooler = false;
        control_cooler(false);
        last = nowMillis;
        Serial.printf("[RULE_ENGINE] 💨 Fan OFF — water cooled (%.2f°C)\n", waterTemp);
    }
}

void checkHeaterLogic(ActuatorState &actuators, float waterTemp, unsigned long nowMillis)
{
    if (isnan(waterTemp)) return;
    if (!actuators.heaterAuto) return;

    static unsigned long last = 0;
    const unsigned long DEBOUNCE_MS = 5000;

    // Handle AUTO re-enable
    if (actuators.heaterAutoJustEnabled) {
        actuators.heaterAutoJustEnabled = false;

        if (waterTemp <= HEATER_ON_TEMP) { 
            actuators.heater = true;
        } else if (waterTemp >= HEATER_OFF_TEMP) {
            actuators.heater = false;
        }

        control_heater(actuators.heater);
        last = nowMillis;

        Serial.printf("[RULE_ENGINE] 🔥 Heater AUTO re-enabled → %s (waterTemp=%.2f°C)\n",
                      actuators.heater ? "ON" : "OFF", waterTemp);
        return;
    }

    bool tooCold  = (waterTemp <= HEATER_ON_TEMP);
    bool warmEnough = (waterTemp >= HEATER_OFF_TEMP);

    if (tooCold && !actuators.heater && (nowMillis - last) > DEBOUNCE_MS) {
        actuators.heater = true;
        control_heater(true);
        last = nowMillis;
        Serial.printf("[RULE_ENGINE] 🔥 Heater ON — water cold (%.2f°C)\n", waterTemp);
    }
    else if (warmEnough && actuators.heater && (nowMillis - last) > DEBOUNCE_MS) {
        actuators.heater = false;
        control_heater(false);
        last = nowMillis;
        Serial.printf("[RULE_ENGINE] 🔥 Heater OFF — water warmed (%.2f°C)\n", waterTemp);
    }
}

void checkpHPumpLogic(ActuatorState &actuators, float phValue, unsigned long nowMillis) {
    if (isnan(phValue)) return;

    static unsigned long lastDoseStart = 0;
    static unsigned long lastCheckTime = 0;
    static bool pumpRunning = false;

    // --- Stop pump if dose complete ---
    if (pumpRunning && nowMillis - lastDoseStart >= PH_PUMP_RUN_MS) {
        control_ph_pump(false, false);
        actuators.phRaising = false;
        actuators.phLowering = false;
        pumpRunning = false;
        lastCheckTime = nowMillis; // start rest period
        Serial.printf("[AUTO PH] Dose complete - pump OFF\n");
        return; // pump just stopped, skip further checks this loop
    }

    // --- Respect rest period ---
    if (lastCheckTime > 0 && nowMillis - lastCheckTime < PH_REST_PERIOD_MS) return;

    // --- Minimum interval between checks ---
    static unsigned long lastPhCheck = 0;
    if (lastPhCheck > 0 && nowMillis - lastPhCheck < PH_MIN_CHECK_INTERVAL_MS) return;
    lastPhCheck = nowMillis;

    // --- Hysteresis logic ---
    if (phValue < PH_LOW_THRESHOLD && !pumpRunning) {
        control_ph_pump(true, false);   // start pH UP
        actuators.phRaising = true;
        actuators.phLowering = false;
        pumpRunning = true;
        lastDoseStart = nowMillis;
        Serial.printf("[AUTO PH] Low pH (%.2f) → Starting pH UP dose\n", phValue);
    }
    else if (phValue > PH_HIGH_THRESHOLD && !pumpRunning) {
        control_ph_pump(false, true);   // start pH DOWN
        actuators.phRaising = false;
        actuators.phLowering = true;
        pumpRunning = true;
        lastDoseStart = nowMillis;
        Serial.printf("[AUTO PH] High pH (%.2f) → Starting pH DOWN dose\n", phValue);
    }
    else if ((actuators.phRaising && phValue >= PH_LOW_OFF) ||
             (actuators.phLowering && phValue <= PH_HIGH_OFF)) {
        // Turn off pumps if back in safe range
        control_ph_pump(false, false);
        actuators.phRaising = false;
        actuators.phLowering = false;
        pumpRunning = false;
        lastCheckTime = nowMillis; // rest period after turning off
        Serial.printf("[AUTO PH] pH back in safe range (%.2f) → Pumps OFF\n", phValue);
    }
}