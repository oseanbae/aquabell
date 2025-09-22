#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "temp_sensor.h"
#include "ph_sensor.h"
#include "do_sensor.h"
#include "turbidity_sensor.h"
#include "dht_sensor.h"
#include "float_switch.h"
#include "sensor_data.h"
#include "rule_engine.h"
#include "lcd_display.h"
#include "firestore_client.h"
#include "time_utils.h"


// === CONSTANTS ===
#define USE_DHT_MOCK true
#define USE_PH_MOCK true
#define USE_DO_MOCK true
#define USE_TURBIDITY_MOCK true
#define USE_WATERTEMP_MOCK true
#define USE_FLOATSWITCH_MOCK true

#define USE_MOCK_DATA true

#define RTDB_POLL_INTERVAL 1000  // Poll RTDB every 1 second

RealTimeData current = {}; // Initialize all fields to 0/false
ActuatorState actuators = {}; // Initialize all actuator states to false/auto
Commands currentCommands = { // Default to AUTO mode for all actuators
    {true, false}, // fan
    {true, false}, // light
    {true, false}, // pump
    {true, false}  // valve
}; // Initialize all command fields to 0/false

// === FORWARD DECLARATIONS ===
void initAllModules();
bool readSensors(unsigned long now, RealTimeData &data);
void displaySensorReading(const char* label, float value, const char* unit) {
    Serial.printf("[SENSOR] %s = %.2f %s\n", label, value, unit);
}
RealTimeData readMockSensors(unsigned long nowMillis);
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected.");
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("🚀 AquaBell System Starting...");

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    initAllModules();

    // === WiFi Connection ===
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
        delay(500);
        yield(); // prevent watchdog reset
        Serial.print(".");
        wifiAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" connected.");

        firebaseSignIn();

        // Time sync now that WiFi is connected
        Serial.println("🕐 Initializing NTP time sync...");
        bool timeSyncSuccess = syncTimeOncePerBoot(10000); // shorter timeout

        if (timeSyncSuccess) {
            Serial.println("✅ Time sync successful - schedule-based actions enabled");
        } else {
            Serial.println("⚠️ Time sync failed - schedule-based actions suspended until time available");
        }
    } else {
        Serial.println(" failed. Continuing without WiFi.");
    }
    Serial.println("✅ System initialization complete.");
}


// === LOOP ===
void loop() {
    unsigned long nowMillis = millis();

    // Non-blocking WiFi check
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWiFiAttempt = 0;
        if (nowMillis - lastWiFiAttempt >= 30000) { // Try every 30 seconds
            Serial.println("📡 WiFi reconnecting...");
            WiFi.reconnect();
            lastWiFiAttempt = nowMillis;
        }
    }

    // Periodic time sync (every 24 hours when WiFi is available)
    periodicTimeSync();

    // Poll RTDB commands every 1 second for real-time control
    static unsigned long lastRTDBPoll = 0;
    bool commandsChanged = false;
    if (WiFi.status() == WL_CONNECTED && nowMillis - lastRTDBPoll >= RTDB_POLL_INTERVAL) {
        commandsChanged = fetchCommandsFromRTDB(currentCommands);
        lastRTDBPoll = nowMillis;
    }

    // Read all sensors every 10 seconds
    bool sensorsUpdated = false;
    sensorsUpdated = readSensors(nowMillis, current);

    // Update LCD with latest sensor data
    lcd_display_update(current);

    // Apply rules when sensors are updated, commands change, or float switch changes
    if (sensorsUpdated || commandsChanged || is_float_switch_triggered()) {
        // Update LCD with latest sensor data
        lcd_display_update(current);
        
        // 1️⃣ Apply AUTO mode logic: ESP32 overwrites values for AUTO actuators
        // 2️⃣ Apply MANUAL mode logic: Use values from RTDB for MANUAL actuators
        applyRulesWithModeControl(current, actuators, currentCommands, nowMillis);

        // Update relay states in RealTimeData for compatibility
        current.relayStates.fan       = actuators.fan;
        current.relayStates.light     = actuators.light;
        current.relayStates.waterPump = actuators.pump;
        current.relayStates.valve     = actuators.valve;

        // 3️⃣ Always sync AUTO actuator values to RTDB (non-blocking)
        if (WiFi.status() == WL_CONNECTED) {
            syncRelayState(current, currentCommands);
        }
    }

    // Process non-blocking retries for RTDB operations
    if (isRetryInProgress()) {
        processRetry();
    }

    static unsigned long lastLiveUpdate = 0;
    static unsigned long lastBatchLog   = 0;

    const unsigned long liveInterval  = 10000;   // 10 seconds - matches sensor reading
    const unsigned long batchInterval = 600000;  // 10 minutes
    const int batchSize               = 60;      // 60 readings = 10 minutes of data

    static RealTimeData logBuffer[batchSize];
    static int logIndex = 0;

    if (WiFi.status() == WL_CONNECTED) {
        // Live push every 10 seconds (synchronized with sensor readings)
        if (nowMillis - lastLiveUpdate >= liveInterval) {
            // 1️⃣ Push only live sensor data to Firestore
            // Only fields: waterTemp, pH, dissolvedOxygen, turbidityNTU,
            // airTemp, airHumidity, floatTriggered
            pushToFirestoreLive(current);
            lastLiveUpdate = nowMillis;
        }

        // Append to logBuffer only when fresh sensor data is available
        if (sensorsUpdated) {
            logBuffer[logIndex++] = current;
        }

        // Every 10 min OR if buffer is full → push batch
        if ((nowMillis - lastBatchLog >= batchInterval) || (logIndex >= batchSize)) {
            time_t timestamp = getUnixTime();
            if (timestamp > 0) {
                // 2️⃣ Push aggregated 5-min averages (sensor logs) to Firestore
                // Note: pushBatchLogToFirestore is kept for backward compatibility
                pushBatchLogToFirestore(logBuffer, logIndex, timestamp);
            }
            logIndex   = 0;
            lastBatchLog = nowMillis;
        }
    }

    yield();
}

// === INIT HELPERS ===
void initAllModules() {
    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();
    relay_control_init(); 
    lcd_init();
    Serial.println("✅ All modules initialized successfully.");
}

// === UNIFIED SENSOR READING (Every 10 seconds) ===
bool readSensors(unsigned long now, RealTimeData &data) {
    static unsigned long lastSensorRead = 0;
    static bool lastFloatState = false;

    // Check if it's time to read all sensors
    if (now - lastSensorRead < UNIFIED_SENSOR_INTERVAL) {
        return false; // Not time to read yet
    }

    Serial.println("📊 Reading all sensors...");
    bool updated = false;

    // --- Water Temp ---
    float temp = USE_WATERTEMP_MOCK ? 
        25.0 + (rand() % 100) / 10.0 : read_waterTemp();
    
    if (!isnan(temp) && temp > -50.0f && temp < 100.0f) {
        data.waterTemp = temp;
        displaySensorReading("🌡️ Water Temp", data.waterTemp, "°C");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid water temperature");
    }

    // --- pH ---
    float ph = USE_PH_MOCK ? 
        6.5 + (rand() % 50) / 10.0 : read_ph(data.waterTemp);
    
    if (!isnan(ph) && ph > -2.0f && ph < 16.0f) {
        data.pH = ph;
        displaySensorReading("🧪 pH", data.pH, "");
        updated = true;
    } else {
        static unsigned long lastPHError = 0;
        if (now - lastPHError >= 60000) { // Log error only once per minute
            Serial.println("⚠️ Invalid pH");
            lastPHError = now;
        }
    }

    // --- Dissolved Oxygen ---
    float doValue = USE_DO_MOCK ? 
        5.0 + (rand() % 40) / 10.0 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
    
    if (!isnan(doValue) && doValue >= -5.0f && doValue < 25.0f) {
        data.dissolvedOxygen = doValue;
        displaySensorReading("🫧 DO", data.dissolvedOxygen, "mg/L");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid DO");
    }

    // --- Turbidity ---
    float turbidity = USE_TURBIDITY_MOCK ? 
        (rand() % 1000) / 10.0 : read_turbidity();
    
    if (!isnan(turbidity) && turbidity >= -100.0f && turbidity < 3000.0f) {
        data.turbidityNTU = turbidity;
        displaySensorReading("🌊 Turbidity", data.turbidityNTU, "NTU");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid turbidity");
    }

    // --- DHT (Air Temp & Humidity) ---
    float airTemp, airHumidity;
    
    if (USE_DHT_MOCK) {
        airTemp = 20.0 + (rand() % 150) / 10.0;
        airHumidity = 40.0 + (rand() % 600) / 10.0;
    } else {
        airTemp = read_dhtTemp();
        airHumidity = read_dhtHumidity();
    }
    
    if (!isnan(airTemp) && !isnan(airHumidity) && 
        airTemp > -40.0f && airTemp < 80.0f &&
        airHumidity >= 0.0f && airHumidity <= 100.0f) {
        data.airTemp = airTemp;
        data.airHumidity = airHumidity;
        displaySensorReading("🌡️ Air Temp", data.airTemp, "°C");
        displaySensorReading("💨 Air Humidity", data.airHumidity, "%");
        updated = true;
    } else {
        Serial.println("⚠️ Invalid DHT");
    }

    // --- Float Switch ---
    bool floatState;
    if (USE_FLOATSWITCH_MOCK) {
        floatState = false;  // default safe state so valve doesn’t open
    } else {
        floatState = float_switch_active();
    }

    if (floatState != lastFloatState) {
        data.floatTriggered = floatState;
        Serial.printf("💧 Float Switch: %s\n", floatState ? "TRIGGERED" : "NORMAL");
        lastFloatState = floatState;
        updated = true;
    }

    // Update last read time
    lastSensorRead = now;
    
    if (updated) {
        Serial.println("✅ All sensors read successfully");
    }
    
    return updated;
}
// Simple phase-based mock
RealTimeData readMockSensors(unsigned long nowMillis) {
    static int phase = 0;
    RealTimeData data;

    // Phase 0-2: Normal conditions
    if (phase < 2) {
        data.waterTemp = 28.0;  // safe
        data.pH = 7.0;
        data.dissolvedOxygen = 7.5;
        data.turbidityNTU = 20.0;
        data.airTemp = 26.0;
        data.airHumidity = 60.0;
        data.floatTriggered = false;
    }
    // Phase 3: Overheat/high humidity → should trigger FAN
    else if (phase == 2) {
        data = {28.0, 7.0, 7.5, 20.0, 32.0, 95.0, false, {}}; 
    }
    // Phase 4: Back to normal → fan should stay ON until min runtime expires
    else {
        data = {28.0, 7.0, 7.5, 20.0, 25.0, 65.0, false, {}}; 
    }


    // Advance slowly, not every loop
    static unsigned long lastSwitch = 0;
    if (nowMillis - lastSwitch > 15000) {  // switch every 15s
        phase++;
        if (phase > 3) phase = 0;  // loop phases
        lastSwitch = nowMillis;
    }

    return data;
}
