#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "float_switch.h"
#include <time.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

unsigned long (*getMillis)() = millis;

#define PUMP_OVERRIDE_DURATION  300000UL
#define PUMP_OVERRIDE_COOLDOWN   60000UL

// RTDB Configuration
#define RTDB_URL "https://aquabell-cap2025-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DEVICE_ID "aquabell_esp32"

// External token from firestore_client
extern String idToken;

// Relay state structure for RTDB
struct RelayState {
    bool isAuto;
    bool value;
};

struct RelayStates {
    RelayState fan;
    RelayState light;
    RelayState pump;
    RelayState valve;
};

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

// === RTDB Functions ===
bool fetchRelayStates(RelayStates& states) {
    if (idToken == "") {
        Serial.println("[RTDB] No auth token available");
        return false;
    }

    String url = String(RTDB_URL) + "/commands/" DEVICE_ID ".json?auth=" + idToken;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, url);

    int httpResponseCode = https.GET();
    if (httpResponseCode != 200) {
        Serial.printf("[RTDB] Failed to fetch relay states: %s\n", https.errorToString(httpResponseCode).c_str());
        https.end();
        return false;
    }

    String response = https.getString();
    https.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.print("[RTDB] JSON parse error: ");
        Serial.println(err.f_str());
        return false;
    }

    if (doc.isNull()) {
        Serial.println("[RTDB] No data received");
        return false;
    }

    // Parse relay states
    if (!doc["fan"].isNull()) {
        states.fan.isAuto = doc["fan"]["isAuto"].as<bool>();
        states.fan.value = doc["fan"]["value"].as<bool>();
    }
    if (!doc["light"].isNull()) {
        states.light.isAuto = doc["light"]["isAuto"].as<bool>();
        states.light.value = doc["light"]["value"].as<bool>();
    }
    if (!doc["pump"].isNull()) {
        states.pump.isAuto = doc["pump"]["isAuto"].as<bool>();
        states.pump.value = doc["pump"]["value"].as<bool>();
    }
    if (!doc["valve"].isNull()) {
        states.valve.isAuto = doc["valve"]["isAuto"].as<bool>();
        states.valve.value = doc["valve"]["value"].as<bool>();
    }

    return true;
}

// === FAN LOGIC ===
void check_climate_and_control_fan(float airTemp, float humidity, int currentMinutes, const RelayState& fanState) {
    // ← ADDED: Sensor validation
    if (isnan(airTemp) || isnan(humidity)) {
        Serial.println("⚠️ Invalid sensor data for fan control");
        return;
    }

    // Check if fan is in manual mode
    if (!fanState.isAuto) {
        control_fan(fanState.value);
        Serial.printf("🌀 Fan MANUAL — Value: %s\n", fanState.value ? "ON" : "OFF");
        return;
    }

    // Auto mode - apply rules
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
        Serial.printf("🌀 Fan AUTO ON — Temp: %.1f°C, Humidity: %.1f%%\n", airTemp, humidity);
    } else if (shouldTurnOff && millis_elapsed(now, fanOnSince) >= FAN_MINUTE_RUNTIME) {
        fanActive = false;
        control_fan(false);
        Serial.println("✅ Fan AUTO OFF — Conditions normal or max runtime exceeded");
    }
}

// === PUMP LOGIC WITH FLOAT SWITCH ===
void check_and_control_pump(float waterTemp, bool waterLevelLow, const RelayState& pumpState) {
    // ← ADDED: Sensor validation
    if (isnan(waterTemp)) {
        Serial.println("⚠️ Invalid water temperature for pump control");
        return;
    }

    // Check if pump is in manual mode
    if (!pumpState.isAuto) {
        control_pump(pumpState.value);
        Serial.printf("🔄 Pump MANUAL — Value: %s\n", pumpState.value ? "ON" : "OFF");
        return;
    }

    // Auto mode - apply rules
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
        Serial.println("🚰 Emergency refill active — water level low");
        return;
    }

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
        Serial.println("🔄 Pump AUTO cycle starting");
    } else if (pumpScheduledOn && millis_elapsed(now, lastPumpToggle) >= (PUMP_ON_DURATION * 60000UL)) {
        pumpScheduledOn = false;
        lastPumpToggle = now;
        control_pump(false);
        Serial.println("⏸️ Pump AUTO cycle pausing");
    }
}

// === LIGHT LOGIC ===
void check_and_control_light(const struct tm& now, const RelayState& lightState) {
    // Check if light is in manual mode
    if (!lightState.isAuto) {
        control_light(lightState.value);
        Serial.printf("💡 Light MANUAL — Value: %s\n", lightState.value ? "ON" : "OFF");
        return;
    }

    // Auto mode - apply time-based rules
    int mins = now.tm_hour * 60 + now.tm_min;
    bool on = (mins >= LIGHT_MORNING_ON && mins < LIGHT_MORNING_OFF) ||
              (mins >= LIGHT_EVENING_ON && mins < LIGHT_EVENING_OFF);
    control_light(on);
    Serial.printf("💡 Light AUTO — Value: %s (Time: %02d:%02d)\n", on ? "ON" : "OFF", now.tm_hour, now.tm_min);
}

// === VALVE LOGIC ===
void check_and_control_valve(bool waterLevelLow, const RelayState& valveState) {
    // Check if valve is in manual mode
    if (!valveState.isAuto) {
        control_valve(valveState.value);
        Serial.printf("🚰 Valve MANUAL — Value: %s\n", valveState.value ? "ON" : "OFF");
        return;
    }

    // Auto mode - emergency refill logic
    if (waterLevelLow) {
        control_valve(true);    // Start refill valve
        Serial.println("🚰 Valve AUTO — Emergency refill active (water level low)");
    } else {
        control_valve(false);   // Stop refill if water level is OK
        Serial.println("🚰 Valve AUTO — Water level OK, refill stopped");
    }
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

// === MAIN DISPATCH ===
void apply_rules(const RealTimeData& current, const struct tm& now) {
    int currentMinutes = now.tm_hour * 60 + now.tm_min;

    // ← FIXED: Use data from current instead of calling float_switch_active() twice
    bool waterLevelLow = current.floatTriggered;

    // Fetch relay states from RTDB
    RelayStates relayStates;
    if (!fetchRelayStates(relayStates)) {
        Serial.println("⚠️ Failed to fetch relay states, using emergency rules");
        apply_emergency_rules(current);
        return;
    }

    // Fan control
    check_climate_and_control_fan(current.airTemp, current.airHumidity, currentMinutes, relayStates.fan);

    // Pump control
    check_and_control_pump(current.waterTemp, waterLevelLow, relayStates.pump);

    // Light control
    check_and_control_light(now, relayStates.light);

    // Valve control
    check_and_control_valve(waterLevelLow, relayStates.valve);
}
