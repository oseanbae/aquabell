#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include "RTClib.h"

unsigned long (*getMillis)() = millis;

// === Constants ===
#define BUZZER_ALERT_FREQ       1000
#define BUZZER_ALERT_DURATION   1000
#define BUZZER_FLOAT_FREQ       1200
#define BUZZER_FLOAT_DURATION   500
#define BUZZER_ACTUATOR_FREQ    2000
#define BUZZER_ACTUATOR_DURATION 150

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

void playTone(int freq, int duration) {
    tone(BUZZER_PIN, freq, duration);
}

// === FAN LOGIC ===
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes) {
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
        playTone(BUZZER_ACTUATOR_FREQ, BUZZER_ACTUATOR_DURATION);
        Serial.println("🌀 Fan ON — Triggered by climate or emergency");
    } else if (shouldTurnOff && millis_elapsed(now, fanOnSince) >= FAN_MINUTE_RUNTIME) {
        fanActive = false;
        control_fan(false);
        playTone(BUZZER_ACTUATOR_FREQ, BUZZER_ACTUATOR_DURATION);
        Serial.println("✅ Fan OFF — Conditions normal or max runtime exceeded");
    }
}

// === PUMP LOGIC WITH FLOAT SWITCH ===
void check_and_control_pump(float waterTemp, bool waterLevelLow) {
    unsigned long now = getMillis();

    // === Handle High Temp Override ===
    if (waterTemp > 30.0 && !overrideActive && millis_elapsed(now, lastOverrideTime) > PUMP_OVERRIDE_COOLDOWN) {
        overrideActive = true;
        lastOverrideTime = now;
        Serial.println("⚠️ Pump override ON — water > 30°C");
        playTone(BUZZER_ACTUATOR_FREQ, BUZZER_ACTUATOR_DURATION);
    }
    if (overrideActive && millis_elapsed(now, lastOverrideTime) >= PUMP_OVERRIDE_DURATION) {
        overrideActive = false;
        Serial.println("✅ Pump override OFF — duration complete");
        playTone(BUZZER_ACTUATOR_FREQ, BUZZER_ACTUATOR_DURATION);
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

    if (!pumpScheduledOn && millis_elapsed(now, lastPumpToggle) >= PUMP_OFF_DURATION) {
        pumpScheduledOn = true;
        lastPumpToggle = now;
        control_pump(true);
    } else if (pumpScheduledOn && millis_elapsed(now, lastPumpToggle) >= PUMP_ON_DURATION) {
        pumpScheduledOn = false;
        lastPumpToggle = now;
        control_pump(false);
    }
}

// === LIGHT LOGIC ===
void check_and_control_light(DateTime now) {
    int mins = now.hour() * 60 + now.minute();
    bool on = (mins >= LIGHT_MORNING_ON && mins < LIGHT_MORNING_OFF) ||
              (mins >= LIGHT_EVENING_ON && mins < LIGHT_EVENING_OFF);
    control_light(on);
}

// === BUZZER ALERT ===
void trigger_alert_if_needed(float turbidity, float waterTemp, float pH, float DOmgL) {
    bool alert = turbidity > 800.0 || waterTemp > 32.0 || pH < 5.5 || pH > 8.5 || DOmgL < 3.0;
    if (alert) {
        playTone(BUZZER_ALERT_FREQ, BUZZER_ALERT_DURATION);
        Serial.println("🚨 ALERT: Sensor reading out of safe range");
    }
}

// === MAIN DISPATCH ===
void apply_rules(const RealTimeData& current, const DateTime& now) {
    Serial.println("========== RULES DEBUG START ==========");
    int currentMinutes = now.hour() * 60 + now.minute();

    // Continuous water level detection
    bool waterLevelLow = float_switch_active();

    // Always log readings
    Serial.printf(
        "[DEBUG] Time=%02d:%02d | WT=%.2f°C | AT=%.2f°C | RH=%.2f%% | pH=%.2f | DO=%.2f | Turb=%.2f | Float=%d\n",
        now.hour(), now.minute(),
        current.waterTemp,
        current.airTemp,
        current.airHumidity,
        current.pH,
        current.dissolvedOxygen,
        current.turbidityNTU,
        waterLevelLow
    );

    // Alert only once when water level first drops
    if (is_float_switch_triggered()) {
        Serial.println("💧 Float switch triggered — low water level");
        playTone(BUZZER_FLOAT_FREQ, BUZZER_FLOAT_DURATION);
    } else {
        Serial.println("[DEBUG] Water level normal.");
    }

    // Fan control
    check_climate_and_control_fan(current.airTemp, current.airHumidity, currentMinutes);

    // Pump + emergency refill
    check_and_control_pump(current.waterTemp, waterLevelLow);

    // Light control
    check_and_control_light(now);

    // Safety alerts
    trigger_alert_if_needed(
        current.turbidityNTU,
        current.waterTemp,
        current.pH,
        current.dissolvedOxygen
    );
    Serial.println("========== RULES DEBUG END ==========");
}
