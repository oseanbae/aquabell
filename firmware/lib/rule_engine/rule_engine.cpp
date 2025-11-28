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
void applyRulesWithModeControl(RealTimeData& data,
    ActuatorState& actuators,
    Commands& commands,
    unsigned long nowMillis) {
    Serial.println("[RULE_ENGINE] Applying mode-aware rules");

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
    static bool prevPhDosingEnabled = true;

    bool fanAutoTransition   = commands.fan.isAuto   && !prevFanAuto;
    bool lightAutoTransition = commands.light.isAuto && !prevLightAuto;
    bool pumpAutoTransition  = commands.pump.isAuto  && !prevPumpAuto;
    bool valveAutoTransition = commands.valve.isAuto && !prevValveAuto;
    bool coolerAutoTransition = commands.cooler.isAuto && !prevCoolerAuto;
    bool heaterAutoTransition = commands.heater.isAuto && !prevHeaterAuto;
    bool phDosingTransition = commands.phDosing.phDosingEnabled && !prevPhDosingEnabled;

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

    // --- COOLER ---
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

    // --- HEATER ---
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

    // --- PH DOSING ---
    if (!commands.phDosing.phDosingEnabled) {
        if (actuators.phDosingEnabled) {
            Serial.println("[RULE_ENGINE] 🧪 pH dosing DISABLED via RTDB — halting automation");
        }
        actuators.phDosingEnabled = false;
        actuators.phDosingJustEnabled = false;
        if (actuators.phRaising || actuators.phLowering) {
            control_ph_pump(false, false);
            actuators.phRaising = false;
            actuators.phLowering = false;
            Serial.println("[RULE_ENGINE] 🧪 Chemical pumps OFF (manual disable)");
        }
    } else {
        if (!actuators.phDosingEnabled) {
            Serial.println("[RULE_ENGINE] 🧪 pH dosing AUTO re-enabled — monitoring pH");
        }
        actuators.phDosingEnabled = true;
        if (phDosingTransition) {
            actuators.phDosingJustEnabled = true;
            Serial.println("[RULE_ENGINE] 🧪 pH dosing toggle detected — resetting algorithm timers");
        }
    }

    // ===== 3. AUTO CONTROL RULES =====
    if (actuators.fanAuto)   checkFanAndGrowLightLogic(actuators, data.airTemp, data.airHumidity, nowMillis);
    if (actuators.lightAuto) checkLightLogic(actuators, nowMillis);
    if (actuators.pumpAuto)  checkPumpLogic(actuators, nowMillis);
    if (actuators.valveAuto) checkValveLogic(actuators, data.floatTriggered, nowMillis);
    if (actuators.coolerAuto) checkCoolerLogic(actuators, data.waterTemp, nowMillis);
    if (actuators.heaterAuto) checkHeaterLogic(actuators, data.waterTemp, nowMillis);

    // Water change and sump cleaning logic (handles both manual and auto internally)
    checkWaterChangeLogic(actuators, commands, data.dissolvedOxygen, nowMillis);
    checkSumpCleaningLogic(actuators, commands, data.turbidityNTU, nowMillis);

    // ===== 4. APPLY FINAL STATES =====
    control_fan(actuators.fan);
    control_light(actuators.light);
    control_pump(actuators.pump);
    control_valve(actuators.valve);
    control_cooler(actuators.cooler);
    control_heater(actuators.heater);

    extern RealTimeData current;
    current.relayStates.fan       = actuators.fan;
    current.relayStates.light     = actuators.light;
    current.relayStates.waterPump = actuators.pump;
    current.relayStates.valve     = actuators.valve;
    current.relayStates.cooler    = actuators.cooler;
    current.relayStates.heater    = actuators.heater;

    Serial.printf("[RULE_ENGINE] Final states - Fan:%s(%s) Light:%s(%s) Pump:%s(%s) Valve:%s(%s) Cooler:%s(%s) Heater:%s(%s)\n",
                  actuators.fan ? "ON" : "OFF", actuators.fanAuto ? "AUTO" : "MANUAL",
                  actuators.light ? "ON" : "OFF", actuators.lightAuto ? "AUTO" : "MANUAL",
                  actuators.pump ? "ON" : "OFF", actuators.pumpAuto ? "AUTO" : "MANUAL",
                  actuators.valve ? "ON" : "OFF", actuators.valveAuto ? "AUTO" : "MANUAL",
                  actuators.cooler ? "ON" : "OFF", actuators.coolerAuto ? "AUTO" : "MANUAL",
                  actuators.heater ? "ON" : "OFF", actuators.heaterAuto ? "AUTO" : "MANUAL");

    // Update previous state trackers
    prevFanAuto   = commands.fan.isAuto;
    prevLightAuto = commands.light.isAuto;
    prevPumpAuto  = commands.pump.isAuto;
    prevValveAuto = commands.valve.isAuto;
    prevCoolerAuto = commands.cooler.isAuto;
    prevHeaterAuto = commands.heater.isAuto;
    prevPhDosingEnabled = commands.phDosing.phDosingEnabled;
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

void checkFanAndGrowLightLogic(ActuatorState& actuators, float t, float h, unsigned long now) {
    if (isnan(t) || isnan(h)) return;

    static unsigned long last = 0;
    const unsigned long DEBOUNCE = 5000;

    // Auto re-enable sync for fan
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

    // --- Fan Logic ---
    bool tooHot = t >= TEMP_ON_THRESHOLD;          // 29C
    bool coolEnough = t <= TEMP_OFF_THRESHOLD;     // 26C
    bool tooHumid = h >= HUMIDITY_MAX_THRESHOLD;   // 70%
    bool dryEnough = h <= HUMIDITY_MIN_THRESHOLD;  // 60%

    bool turnOnFan = tooHot || tooHumid;
    bool turnOffFan = coolEnough && dryEnough;

    // Fan control logic
    if (turnOnFan && !actuators.fan && now - last > DEBOUNCE) {
        actuators.fan = true;
        control_fan(true);
        last = now;
        Serial.printf("[RULE_ENGINE] Fan ON (T=%.1f, H=%.1f)\n", t, h);
    }
    else if (turnOffFan && actuators.fan && now - last > DEBOUNCE) {
        actuators.fan = false;
        control_fan(false);
        last = now;
        Serial.printf("[RULE_ENGINE] Fan OFF (T=%.1f, H=%.1f)\n", t, h);    
    }

    // --- Skip Light Control if it's Managed by the Schedule (checkLightLogic) ---
    if (actuators.lightAuto && actuators.light) {
        // Light is on due to schedule, skip grow light logic
        Serial.println("[RULE_ENGINE] Skipping Grow Light Control - Light ON due to schedule");
        return;
    }

    // --- Grow Light Control Logic based on Temperature Thresholds ---
    bool turnOnGrowLight = t <= TEMP_LOW_THRESHOLD;     // Temperature below 18°C
    bool turnOffGrowLight = t >= TEMP_HIGH_THRESHOLD;   // Temperature above 24°C

    // Trigger grow light (only if it's not controlled by schedule)
    if (turnOnGrowLight && !actuators.light && now - last > DEBOUNCE) {
        actuators.light = true;
        control_light(true);
        last = now;
        Serial.printf("[RULE_ENGINE] Grow Light ON (T=%.1f)\n", t);
    }
    else if (turnOffGrowLight && actuators.light && now - last > DEBOUNCE) {
        actuators.light = false;
        control_light(false);
        last = now;
        Serial.printf("[RULE_ENGINE] Grow Light OFF (T=%.1f)\n", t);    
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

    static unsigned long lastDoseEnd = 0;
    static unsigned long lastPhCheck = 0;
    static bool dosingInProgress = false;
    static int dosingAttempts = 0;

    if (!actuators.phDosingEnabled) {
        if (dosingInProgress || actuators.phRaising || actuators.phLowering) {
            control_ph_pump(false, false);
            actuators.phRaising = false;
            actuators.phLowering = false;
            dosingInProgress = false;
            Serial.println("[AUTO PH] Disabled — chemical pumps OFF");
        }
        return;
    }

    if (actuators.phDosingJustEnabled) {
        actuators.phDosingJustEnabled = false;
        dosingInProgress = false;
        dosingAttempts = 0;
        lastDoseEnd = 0;
        lastPhCheck = 0;
        Serial.println("[AUTO PH] Dosing automation re-enabled — timers reset");
    }

    // --- Stop if within rest period ---
    if (lastDoseEnd > 0 && nowMillis - lastDoseEnd < PH_REST_PERIOD_MS) return;

    // --- Limit how often to check ---
    if (lastPhCheck > 0 && nowMillis - lastPhCheck < PH_MIN_CHECK_INTERVAL_MS) return;
    lastPhCheck = nowMillis;

    // --- Handle pH UP dosing ---
    if (phValue < PH_LOW_THRESHOLD && !dosingInProgress) {
        dosingInProgress = true;
        actuators.phRaising = true;
        actuators.phLowering = false;

        Serial.printf("[AUTO PH] Low pH (%.2f) → Starting gradual UP dosing\n", phValue);

        for (int i = 0; i < PH_PULSE_COUNT; i++) {
            control_ph_pump(true, false);
            delay(PH_PULSE_MS);
            control_ph_pump(false, false);

            Serial.printf("[AUTO PH] UP pulse %d/%d complete\n", i + 1, PH_PULSE_COUNT);
            delay(PH_PULSE_GAP_MS);
        }

        dosingInProgress = false;
        dosingAttempts++;
        lastDoseEnd = nowMillis;
    }

    // --- Handle pH DOWN dosing ---
    else if (phValue > PH_HIGH_THRESHOLD && !dosingInProgress) {
        dosingInProgress = true;
        actuators.phRaising = false;
        actuators.phLowering = true;

        Serial.printf("[AUTO PH] High pH (%.2f) → Starting gradual DOWN dosing\n", phValue);

        for (int i = 0; i < PH_PULSE_COUNT; i++) {
            control_ph_pump(false, true);
            delay(PH_PULSE_MS);
            control_ph_pump(false, false);

            Serial.printf("[AUTO PH] DOWN pulse %d/%d complete\n", i + 1, PH_PULSE_COUNT);
            delay(PH_PULSE_GAP_MS);
        }

        dosingInProgress = false;
        dosingAttempts++;
        lastDoseEnd = nowMillis;
    }

    // --- Stop when pH returns to safe range ---
    else if ((actuators.phRaising && phValue >= PH_LOW_OFF) ||
             (actuators.phLowering && phValue <= PH_HIGH_OFF)) {
        control_ph_pump(false, false);
        actuators.phRaising = false;
        actuators.phLowering = false;
        dosingInProgress = false;
        dosingAttempts = 0;
        lastDoseEnd = nowMillis;

        Serial.printf("[AUTO PH] pH back in safe range (%.2f) → Pumps OFF\n", phValue);
    }

    // --- Safety: too many consecutive doses ---
    if (dosingAttempts >= PH_MAX_DOSING_ATTEMPTS) {
        Serial.println("[AUTO PH] ⚠️ Too many consecutive doses. Forcing rest period.");
        dosingAttempts = 0;
        lastDoseEnd = nowMillis; // enforce rest
    }
}

void checkWaterChangeLogic(ActuatorState& actuators, Commands& commands, float doValue, unsigned long nowMillis) {
    if (isnan(doValue)) return;

    static bool manualDrainActive = false;
    static unsigned long manualDrainStart = 0;
    static bool refillActive = false;
    static unsigned long refillStart = 0;
    static bool prevDrainState = false;
    static bool prevValveState = false;
    static unsigned long lowSince = 0;
    static bool intentPrinted = false;
    static unsigned long lastDrainTs = 0;

    // --- Sync on AUTO re-enable ---
    if (actuators.waterChangeAutoJustEnabled) {
        actuators.waterChangeAutoJustEnabled = false;
        manualDrainActive = false;
        refillActive = false;
        lowSince = 0;
        intentPrinted = false;
        lastDrainTs = 0;
        commands.waterChange.inProgress = false;
        Serial.println("[RULE_ENGINE] ♻️ Drain AUTO re-enabled — timers reset");
    }

    // --- MANUAL DRAIN START ---
    if (commands.waterChange.manualChangeRequest && !manualDrainActive && !actuators.pump) {
        manualDrainActive = true;
        manualDrainStart = nowMillis;
        actuators.waterChange = true;
        actuators.valve = false;  // Ensure valve is closed during drain
        refillActive = false;
        commands.waterChange.inProgress = true;
        control_drain(true);
        control_valve(false);
        Serial.println("[MANUAL] Manual water change requested — starting drain cycle");
    }

    // --- MANUAL DRAIN CANCEL ---
    if (manualDrainActive && commands.waterChange.manualChangeCancel) {
        actuators.waterChange = false;
        control_drain(false);
        manualDrainActive = false;
        refillActive = false;
        commands.waterChange.inProgress = false;
        Serial.println("[MANUAL] Manual water change canceled by user");
    }

    // --- MANUAL DRAIN AUTO STOP / START REFILL ---
    if (manualDrainActive && (nowMillis - manualDrainStart >= DO_DRAIN_DURATION_MS)) {
        actuators.waterChange = false;
        control_drain(false);
        manualDrainActive = false;
        refillActive = true;
        refillStart = nowMillis;
        actuators.valve = true;  // open valve for refill
        control_valve(true);
        commands.waterChange.inProgress = true;
        Serial.println("[MANUAL] Manual drain cycle complete — starting refill");
    }

    // --- AUTO REFILL LOGIC ---
    if (refillActive) {
        // Pause refill if growbed pump is running
        if (actuators.pump) {
            actuators.valve = false;
            control_valve(false);
            Serial.println("[MANUAL] Refill paused — growbed pump active");
        } else {
            actuators.valve = true;
            control_valve(true);
            // Stop refill if water level high or max refill time exceeded
            if (nowMillis - refillStart >= MAX_REFILL_MS) {
                actuators.valve = false;
                control_valve(false);
                refillActive = false;
                commands.waterChange.inProgress = false;
                Serial.println("[MANUAL] Refill complete — valve closed");
            }
        }
    }

    // --- AUTO DO DRAIN (if no manual drain or refill) ---
    if (!manualDrainActive && !refillActive && actuators.waterChangeAuto) {
        if (doValue <= DO_LOW_THRESHOLD) {
            if (lowSince == 0) lowSince = nowMillis;
            if (!intentPrinted && !actuators.waterChange) {
                Serial.printf("[RULE_ENGINE] [INTENT] DO low (%.2f mg/L) — drain about to run\n", doValue);
                intentPrinted = true;
            }
        } else {
            lowSince = 0;
            intentPrinted = false;
        }

        bool lowStable = (lowSince > 0) && (nowMillis - lowSince >= DO_LOW_DEBOUNCE_MS);

        if (lowStable && !actuators.waterChange && !actuators.pump) { // check growbed pump
            actuators.waterChange = true;
            lastDrainTs = nowMillis;
            commands.waterChange.inProgress = true;
            Serial.printf("[RULE_ENGINE] ♻️ Drain ON — DO critically low (%.2f mg/L)\n", doValue);
            control_drain(true);
        }

        // Stop after duration
        if (actuators.waterChange && (nowMillis - lastDrainTs >= DO_DRAIN_DURATION_MS)) {
            actuators.waterChange = false;
            lastDrainTs = nowMillis;
            lowSince = 0;
            intentPrinted = false;
            commands.waterChange.inProgress = false;
            Serial.println("[RULE_ENGINE] ♻️ Drain OFF — cycle complete");
            control_drain(false);
        }
    }

    // --- APPLY RELAYS ---
    if (actuators.waterChange != prevDrainState) {
        control_drain(actuators.waterChange);
        prevDrainState = actuators.waterChange;
    }

    if (actuators.valve != prevValveState) {
        control_valve(actuators.valve);
        prevValveState = actuators.valve;
    }
}

void checkSumpCleaningLogic(ActuatorState &actuators, Commands& commands,
                            float turbidityNTU, unsigned long nowMillis)
{
    if (isnan(turbidityNTU)) return;

    static bool manualCleaningActive = false;
    static unsigned long manualCleaningStart = 0;

    static unsigned long highSince = 0;
    static unsigned long cleaningStart = 0;
    static bool cleaningActive = false;
    static bool intentPrinted = false;

    //         AUTO RE-ENABLE RESET
    if (actuators.sumpCleaningAutoJustEnabled) {
        actuators.sumpCleaningAutoJustEnabled = false;

        manualCleaningActive = false;
        highSince = 0;
        cleaningStart = 0;
        cleaningActive = false;
        intentPrinted = false;
        commands.sumpCleaning.inProgress = false;

        Serial.println("[SUMP] Auto re-enabled — timers reset");
    }

    //                   MANUAL START
    if (commands.sumpCleaning.manualCleanRequest && !manualCleaningActive) {

        Serial.println("[SUMP][MANUAL] Manual cleaning requested — starting");

        manualCleaningActive = true;
        manualCleaningStart = nowMillis;

        actuators.sumpCleaning = true;
        cleaningActive = false;         // disable auto cleaning
        highSince = 0;
        intentPrinted = false;
        commands.sumpCleaning.inProgress = true;
        control_sump_cleaning(true, true);   // open both valves
    }

    //                     MANUAL CANCEL
    if (manualCleaningActive && commands.sumpCleaning.manualCleanCancel) {

        Serial.println("[SUMP][MANUAL] Manual cleaning canceled");

        manualCleaningActive = false;
        actuators.sumpCleaning = false;
        commands.sumpCleaning.inProgress = false;
        control_sump_cleaning(false, false);  // close both valves
        return;
    }

    //      MANUAL CLEANING AUTO STOP AFTER DURATION
    if (manualCleaningActive &&
        nowMillis - manualCleaningStart >= SUMP_CLEAN_DURATION_MS)
    {
        Serial.println("[SUMP][MANUAL] Manual cycle complete — stopping");

        manualCleaningActive = false;
        actuators.sumpCleaning = false;
        commands.sumpCleaning.inProgress = false;
        control_sump_cleaning(false, false);
        return;
    }

    // If manual is in progress → AUTO MUST NOT RUN
    if (manualCleaningActive) return;

    //                     AUTO MODE BELOW
    if (!actuators.sumpCleaningAuto) return;

    // --- Turbidity tracking ---
    if (turbidityNTU >= TURBIDITY_CLEAN_THRESHOLD) {
        if (highSince == 0) highSince = nowMillis;

        if (!intentPrinted && !cleaningActive) {
            Serial.printf("[SUMP] High NTU detected (%.2f). Cleaning pending…\n",
                          turbidityNTU);
            intentPrinted = true;
        }
    } else {
        highSince = 0;
        intentPrinted = false;
    }

    // --- Auto start ---
    if (!cleaningActive) {

        Serial.printf("[SUMP] AUTO START — NTU=%.2f\n", turbidityNTU);

        cleaningActive = true;
        cleaningStart = nowMillis;
        actuators.sumpCleaning = true;
        commands.sumpCleaning.inProgress = true;

        control_sump_cleaning(true, true);
    }

    // --- End auto cleaning ---
    if (cleaningActive &&
        nowMillis - cleaningStart >= SUMP_CLEAN_DURATION_MS)
    {
        Serial.println("[SUMP] AUTO DONE — closing valves");

        cleaningActive = false;
        actuators.sumpCleaning = false;
        commands.sumpCleaning.inProgress = false;

        control_sump_cleaning(false, false);
    }
}
