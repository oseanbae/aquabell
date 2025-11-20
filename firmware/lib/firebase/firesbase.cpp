#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FirebaseClient.h>
#include "config.h"
#include "sensor_data.h"
#include "relay_control.h"
#include "time_utils.h"
#include "firebase.h"
#include "rule_engine.h"
#include <time.h>

#define FIREBASE_PROJECT_ID "aquabell-cap2025"
#define DEVICE_ID "aquabell_esp32"
#define FIREBASE_API_KEY "AIzaSyCOPHwBDym3PNQFXQX9MqiojBSO4mnnIb4"
#define USER_EMAIL "esp32@test.com"
#define USER_PASSWORD "cap413"

String idToken = "";
String refreshToken = "";
unsigned long tokenExpiryTime = 0;

// FirebaseClient global objects
WiFiClientSecure ssl_client;
AsyncClientClass aClient;
FirebaseApp app;
RealtimeDatabase Database;

bool streamConnected = false;
unsigned long lastStreamReconnectAttempt = 0;
const unsigned long STREAM_RECONNECT_INTERVAL = 30000; // 30 seconds
static bool firebaseAppInitialized = false;
static bool databaseConfigured = false;
static DefaultNetwork defaultNet; // provides network_config_data for built-in WiFi

// Readiness/sync flags
static bool firebaseReady = false;                 // true when app.ready() once
static bool initialCommandsSynced = false;         // true after first /commands fetch or full snapshot
static bool needResyncOnReconnect = true;          // fetch commands when connectivity/auth returns

// Forward declarations
extern void evaluateRules(bool forceImmediate);

// Non-blocking retry state for RTDB PATCH requests
struct RetryState {
    bool isRetrying = false;
    unsigned long lastAttemptTime = 0;
    int attemptCount = 0;
    String payload = "";
    String url = "";
    unsigned long retryInterval = 5000; // 5 seconds between retries
    int maxRetries = 3;
    bool isRelaySync = false; // true for syncRelayState, false for other operations
};
static RetryState retryState;

bool tokenValid = true;
unsigned long lastTokenAttempt = 0;

// ==========================================================
//  AUTH CORE LOGIC
// ==========================================================

bool firebaseSignIn() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" FIREBASE_API_KEY);
    https.addHeader("Content-Type", "application/json");

    String payload = "{\"email\":\"" USER_EMAIL "\",\"password\":\"" USER_PASSWORD "\",\"returnSecureToken\":true}";

    int httpResponseCode = https.POST(payload);
    if (httpResponseCode == 200) {
        String response = https.getString();
        JsonDocument doc;
        deserializeJson(doc, response);

        idToken = doc["idToken"].as<String>();
        refreshToken = doc["refreshToken"].as<String>();
        int expiresIn = doc["expiresIn"].as<int>();
        tokenExpiryTime = millis() + (expiresIn - 180) * 1000UL;

        firebaseReady = true;
        tokenValid = true;
        Serial.println("[Auth] ✅ Signed in successfully");

        https.end();
        return true;
    } else {
        firebaseReady = false;
        tokenValid = false;
        Serial.printf("[Auth] ❌ Sign-in failed: %s\n", https.errorToString(httpResponseCode).c_str());
        https.end();
        return false;
    }
}

bool refreshIdToken() {
    if (refreshToken == "") {
        Serial.println("[Auth] No refresh token, re-signing in...");
        return firebaseSignIn();
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, "https://securetoken.googleapis.com/v1/token?key=" FIREBASE_API_KEY);
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "grant_type=refresh_token&refresh_token=" + refreshToken;

    int httpResponseCode = https.POST(payload);

    if (httpResponseCode == 200) {
        String response = https.getString();
        JsonDocument doc;
        deserializeJson(doc, response);

        idToken = doc["id_token"].as<String>();
        refreshToken = doc["refresh_token"].as<String>();
        int expiresIn = doc["expires_in"].as<int>();
        tokenExpiryTime = millis() + (expiresIn - 60) * 1000UL; // refresh 1min early

        Serial.println("[Auth] 🔁 Token refreshed successfully");
        https.end();
        return true;
    } else {
        Serial.printf("[Auth] ⚠️ Token refresh failed: %s\n", https.errorToString(httpResponseCode).c_str());
        https.end();
        return false;
    }
}

void safeTokenRefresh() {
    unsigned long now = millis();
    const unsigned long retryInterval = 300000; // 5 minutes

    if (!tokenValid && (now - lastTokenAttempt < retryInterval))
        return; // still within cooldown

    if (millis() > tokenExpiryTime || !tokenValid) {
        Serial.println("[Auth] Refreshing ID token...");
        lastTokenAttempt = now;

        bool refreshed = refreshIdToken();
        if (refreshed) {
            tokenValid = true;
            Serial.println("[Auth] ✅ Token refresh successful");
        } else {
            tokenValid = false;
            Serial.println("[Auth] ⚠️ Token refresh failed — retry in 5min");
        }
    }
}

// ===== NON-BLOCKING RETRY MANAGEMENT =====

// Initialize retry state for a new operation
void initRetryState(const String& url, const String& payload, bool isRelaySync = false) {
    retryState.isRetrying = true;
    retryState.lastAttemptTime = millis();
    retryState.attemptCount = 0;
    retryState.payload = payload;
    retryState.url = url;
    retryState.isRelaySync = isRelaySync;
    Serial.printf("[Retry] Initialized retry state for %s operation\n", 
                  isRelaySync ? "relay sync" : "RTDB");
}

// Check if retry is needed and perform it
bool processRetry() {
    if (!retryState.isRetrying) {
        return false; // No retry in progress
    }

    unsigned long now = millis();
    
    // Check if enough time has passed for next retry
    if (now - retryState.lastAttemptTime < retryState.retryInterval) {
        return true; // Still waiting, but retry is in progress
    }

    // Check if we've exceeded max retries
    if (retryState.attemptCount >= retryState.maxRetries) {
        Serial.printf("[Retry] Max retries (%d) exceeded, giving up\n", retryState.maxRetries);
        retryState.isRetrying = false;
        return false;
    }

    // Check and refresh token before retry
    if (millis() > tokenExpiryTime) {
        Serial.println("[Retry] Token expired, refreshing before retry");
        refreshIdToken();
    }

    // Perform the retry attempt
    retryState.attemptCount++;
    retryState.lastAttemptTime = now;
    
    Serial.printf("[Retry] Attempt %d/%d for %s operation\n", 
                  retryState.attemptCount, retryState.maxRetries,
                  retryState.isRelaySync ? "relay sync" : "RTDB");

    // Check WiFi connection before retry
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Retry] WiFi not connected, skipping retry");
        retryState.isRetrying = false;
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000); // 10 second timeout
    
    HTTPClient https;
    https.setTimeout(10000); // 10 second timeout
    https.begin(client, retryState.url);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + idToken);
    
    int httpResponseCode = https.sendRequest("PATCH", retryState.payload);
    Serial.printf("[Retry] HTTP response: %d\n", httpResponseCode);

    if (httpResponseCode == 200) {
        Serial.printf("[Retry] ✅ Success on attempt %d\n", retryState.attemptCount);
        retryState.isRetrying = false;
    } else {
        String response = https.getString();
        Serial.printf("[Retry] Failed: %s\n", https.errorToString(httpResponseCode).c_str());
        Serial.printf("[Retry] Response body: %s\n", response.c_str());
        
        // Continue retrying if we haven't exceeded max attempts
        if (retryState.attemptCount < retryState.maxRetries) {
            Serial.printf("[Retry] Will retry in %lu ms\n", retryState.retryInterval);
        }
    }
    
    // Always end the connection to free memory
    https.end();

    return true; // Retry still in progress
}

// Check if any retry is currently in progress
bool isRetryInProgress() {
    return retryState.isRetrying;
}

// ===== Helpers for RTDB Stream SSE payloads =====
static bool extractSSEJson(const String &rawPayload, String &outJson) {
    String s = rawPayload;
    s.trim();
    if (s.startsWith("{") || s.startsWith("[")) {
        outJson = s;
        return true;
    }

    int lastIdx = -1;
    int searchFrom = -1;
    // Find the last occurrence of "data:" which should hold the JSON
    while (true) {
        int idx = s.indexOf("data:", searchFrom + 1);
        if (idx == -1) break;
        lastIdx = idx;
        searchFrom = idx;
    }

    if (lastIdx != -1) {
        int start = lastIdx + 5; // skip 'data:'
        // skip one optional space
        while (start < (int)s.length() && s[start] == ' ') start++;
        outJson = s.substring(start);
        outJson.trim();
        return outJson.length() > 0;
    }

    return false;
}

// ===== FIREBASE RTDB & Firestore INTERACTIONS =====
// Push only sensors that were updated
void pushToRTDBLive(const RealTimeData &data, const bool updatedSensors[6]) {
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() > tokenExpiryTime) {
        Serial.println("[RTDB] Token expired, skipping live update");
        return;
    }

    String url = "https://aquabell-cap2025-default-rtdb.asia-southeast1.firebasedatabase.app/live_data/" DEVICE_ID ".json?auth=" + idToken;

    JsonDocument doc;
    if (updatedSensors[0] && !isnan(data.waterTemp)) doc["waterTemp"] = data.waterTemp;
    if (updatedSensors[1] && !isnan(data.pH)) doc["pH"] = data.pH;
    if (updatedSensors[2] && !isnan(data.dissolvedOxygen)) doc["dissolvedOxygen"] = data.dissolvedOxygen;
    if (updatedSensors[3] && !isnan(data.turbidityNTU)) doc["turbidityNTU"] = data.turbidityNTU;
    if (updatedSensors[4]) {
        if (!isnan(data.airTemp)) doc["airTemp"] = data.airTemp;
        if (!isnan(data.airHumidity)) doc["airHumidity"] = data.airHumidity;
    }
    if (updatedSensors[5]) doc["floatTriggered"] = data.floatTriggered;

    // Always update timestamp
    doc["timestamp"] = time(nullptr);

    String payload;
    serializeJson(doc, payload);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, url);
    https.addHeader("Content-Type", "application/json");

    int httpResponseCode = https.PATCH(payload);
    Serial.printf("[RTDB] pushToRTDBLive HTTP response: %d\n", httpResponseCode);

    if (httpResponseCode == 200) {
        Serial.println("[RTDB] Live data updated successfully");
    } else {
        Serial.printf("[RTDB] Update failed: %s\n", https.errorToString(httpResponseCode).c_str());
        Serial.printf("[RTDB] Response body: %s\n", https.getString().c_str());
        Serial.println("[RTDB] Scheduling retry for live data push...");
        initRetryState(url, payload, false);
    }

    https.end();
}

bool pushBatchLogToFirestore(RealTimeData *buffer, int size, time_t timestamp) {
    if (millis() > tokenExpiryTime) {
        Serial.println("[Firestore] Token expired, refreshing before pushBatchLogToFirestore");
        refreshIdToken();
    }

    if (size <= 0) return false;

    // Compute averages (same as before)
    double sumWater = 0, sumPH = 0, sumDO = 0, sumNTU = 0, sumAirT = 0, sumAirH = 0;
    for (int i = 0; i < size; i++) {
        sumWater += buffer[i].waterTemp;
        sumPH += buffer[i].pH;
        sumDO += buffer[i].dissolvedOxygen;
        sumNTU += buffer[i].turbidityNTU;
        sumAirT += buffer[i].airTemp;
        sumAirH += buffer[i].airHumidity;
    }
    int count = size;
    float avgWater = sumWater / count;
    float avgPH = sumPH / count;
    float avgDO = sumDO / count;
    float avgNTU = sumNTU / count;
    float avgAirT = sumAirT / count;
    float avgAirH = sumAirH / count;

    // Create timestamped doc ID
    struct tm timeinfo;
    char dateStr[11];
    if (localtime_r(&timestamp, &timeinfo))
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    else
        strcpy(dateStr, "1970-01-01");

    String url = String("https://firestore.googleapis.com/v1/projects/") +
             FIREBASE_PROJECT_ID +
             "/databases/(default)/documents/sensor_logs/" +
             String(dateStr) +
             "/entries/" +
             String(timestamp);


    JsonDocument doc;
    JsonObject fields = doc["fields"].to<JsonObject>();
    fields["timestamp"]["integerValue"] = (long long)timestamp;
    fields["avgWaterTemp"]["doubleValue"] = avgWater;
    fields["avgPH"]["doubleValue"] = avgPH;
    fields["avgDO"]["doubleValue"] = avgDO;
    fields["avgTurbidityNTU"]["doubleValue"] = avgNTU;
    fields["avgAirTemp"]["doubleValue"] = avgAirT;
    fields["avgAirHumidity"]["doubleValue"] = avgAirH;
    fields["count"]["integerValue"] = count;
    fields["date"]["stringValue"] = dateStr;

    String payload;
    serializeJson(doc, payload);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, url);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = https.sendRequest("PATCH", payload);
    https.end();

    if (httpResponseCode == 200) {
        Serial.println("[Firestore] ✅ Batch log updated successfully");
        return true;
    }

    Serial.printf("[Firestore] ❌ Failed: %d — will retry later\n", httpResponseCode);
    return false;
}

bool fetchControlCommands() {
    // Check and refresh token before RTDB call
    if (millis() > tokenExpiryTime) {
        Serial.println("[RTDB] Token expired, refreshing before fetchControlCommands");
        refreshIdToken();
    }

    String url = "https://aquabell-cap2025-default-rtdb.asia-southeast1.firebasedatabase.app/commands/" DEVICE_ID ".json?auth=" + idToken;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, url);

    int httpResponseCode = https.GET();
    Serial.printf("[RTDB] fetchControlCommands HTTP response: %d\n", httpResponseCode);
    
    if (httpResponseCode != 200) {
        String response = https.getString();
        Serial.printf("[RTDB] Fetch control commands failed: %s\n", https.errorToString(httpResponseCode).c_str());
        Serial.printf("[RTDB] Response body: %s\n", response.c_str());
        https.end();
        
        // Retry once
        delay(5000);
        https.begin(client, url);
        httpResponseCode = https.GET();
        Serial.printf("[RTDB] Retry HTTP response: %d\n", httpResponseCode);
        
        if (httpResponseCode != 200) {
            response = https.getString();
            Serial.printf("[RTDB] Retry failed: %s\n", https.errorToString(httpResponseCode).c_str());
            Serial.printf("[RTDB] Retry response body: %s\n", response.c_str());
            https.end();
            return false;
        }
    }

    String response = https.getString();
    https.end();
    Serial.printf("[RTDB] fetchControlCommands response body: %s\n", response.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.printf("[RTDB] JSON parse error: %s\n", err.c_str());
        return false;
    }

    if (doc.isNull()) {
        Serial.println("[RTDB] No command data received");
        return false;
    }

    bool anyManualControl = false;
    extern Commands currentCommands;

    auto handleActuator = [&](const char* key, CommandState& cmd, void (*controlFn)(bool)) {
        if (doc[key].isNull()) return;
        JsonObject actuatorData = doc[key].as<JsonObject>();
        if (actuatorData.isNull()) return;

        bool isAuto = actuatorData["isAuto"].isNull() ? cmd.isAuto : actuatorData["isAuto"].as<bool>();
        bool value = actuatorData["value"].isNull() ? cmd.value : actuatorData["value"].as<bool>();

        cmd.isAuto = isAuto;
        cmd.value = value;

        Serial.printf("[Control] %s isAuto=%s value=%s\n",
                      key, isAuto ? "true" : "false", value ? "true" : "false");

        if (!isAuto) {
            controlFn(value);
            anyManualControl = true;
            Serial.printf("[Control] Applied manual control for %s\n", key);
        }
    };

    handleActuator("fan", currentCommands.fan, control_fan);
    handleActuator("light", currentCommands.light, control_light);
    handleActuator("pump", currentCommands.pump, control_pump);
    handleActuator("valve", currentCommands.valve, control_valve);
    handleActuator("cooler", currentCommands.cooler, control_cooler);
    handleActuator("heater", currentCommands.heater, control_heater);

    if (!doc["phDosing"].isNull()) {
        JsonObject phObj = doc["phDosing"].as<JsonObject>();
        if (!phObj.isNull()) {
            if (!phObj["phDosingEnabled"].isNull()) {
                currentCommands.phDosing.phDosingEnabled = phObj["phDosingEnabled"].as<bool>();
            }
            if (!phObj["value"].isNull()) {
                currentCommands.phDosing.value = phObj["value"].as<bool>();
            }
            Serial.printf("[Control] phDosing enabled=%s value=%s\n",
                          currentCommands.phDosing.phDosingEnabled ? "true" : "false",
                          currentCommands.phDosing.value ? "true" : "false");
        }
    }

    if (anyManualControl) {
        Serial.println("[Control] Applied manual relay states.");
        return true; // manual control was applied
    }

    // No manual controls found, rule engine should take over
    return false;
}

void syncRelayState(const RealTimeData &data, const Commands &commands) {
    if (!initialCommandsSynced) {
        Serial.println("[RTDB] Skipping relay sync (initial /commands not yet synced)");
        return;
    }

    if (isRetryInProgress()) {
        Serial.println("[RTDB] Retry in progress — skipping syncRelayState");
        return;
    }

    if (millis() > tokenExpiryTime) {
        Serial.println("[RTDB] Token expired, refreshing before syncRelayState");
        refreshIdToken();
    }

    String url = "https://aquabell-cap2025-default-rtdb.asia-southeast1.firebasedatabase.app/commands/" DEVICE_ID ".json?auth=" + idToken;

    JsonDocument doc;

    // Only sync actuators in AUTO mode
    if (commands.fan.isAuto)    doc["fan/value"] = data.relayStates.fan;
    if (commands.light.isAuto)  doc["light/value"] = data.relayStates.light;
    if (commands.pump.isAuto)   doc["pump/value"] = data.relayStates.waterPump;
    if (commands.valve.isAuto)  doc["valve/value"] = data.relayStates.valve;
    if (commands.cooler.isAuto)  doc["cooler/value"] = data.relayStates.cooler;
    if (commands.heater.isAuto)  doc["heater/value"] = data.relayStates.heater;

    bool phActive = data.relayStates.phRaising || data.relayStates.phLowering;
    bool shouldSyncPh = (commands.phDosing.value != phActive);
    if (shouldSyncPh) {
        doc["phDosing/value"] = phActive;
    }

    if (doc.size() == 0) return;  // Nothing to sync

    String payload;
    serializeJson(doc, payload);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, url);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = https.sendRequest("PATCH", payload);

    if (httpResponseCode == 200) {
        Serial.println("[RTDB] Relay sync ✅");
        if (shouldSyncPh) {
            extern Commands currentCommands;
            currentCommands.phDosing.value = phActive;
        }
    } else {
        Serial.printf("[RTDB] Relay sync failed (%d)\n", httpResponseCode);
        initRetryState(url, payload, true);
    }

    https.end();
}

// ===== FIREBASECLIENT STREAM-BASED FUNCTIONS =====
// Helper function to process Firebase patch events
void processPatchEvent(const String& dataPath, JsonDocument& patchData, Commands& currentCommands, bool& hasChanges) {
    // Determine which component was updated based on the path
    if (dataPath == "/fan") {
        // Update fan command
        if (!patchData["isAuto"].isNull()) {
            bool newIsAuto = patchData["isAuto"].as<bool>();
            if (currentCommands.fan.isAuto != newIsAuto) {
                currentCommands.fan.isAuto = newIsAuto;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Fan isAuto: %s\n", newIsAuto ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.fan.value != newValue) {
                currentCommands.fan.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Fan value: %s\n", newValue ? "true" : "false");
            }
        }
    }
    else if (dataPath == "/light") {
        // Update light command
        if (!patchData["isAuto"].isNull()) {
            bool newIsAuto = patchData["isAuto"].as<bool>();
            if (currentCommands.light.isAuto != newIsAuto) {
                currentCommands.light.isAuto = newIsAuto;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Light isAuto: %s\n", newIsAuto ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.light.value != newValue) {
                currentCommands.light.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Light value: %s\n", newValue ? "true" : "false");
            }
        }
    }
    else if (dataPath == "/pump") {
        // Update pump command
        if (!patchData["isAuto"].isNull()) {
            bool newIsAuto = patchData["isAuto"].as<bool>();
            if (currentCommands.pump.isAuto != newIsAuto) {
                currentCommands.pump.isAuto = newIsAuto;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Pump isAuto: %s\n", newIsAuto ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.pump.value != newValue) {
                currentCommands.pump.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Pump value: %s\n", newValue ? "true" : "false");
            }
        }
    }
    else if (dataPath == "/valve") {
        // Update valve command
        if (!patchData["isAuto"].isNull()) {
            bool newIsAuto = patchData["isAuto"].as<bool>();
            if (currentCommands.valve.isAuto != newIsAuto) {
                currentCommands.valve.isAuto = newIsAuto;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Valve isAuto: %s\n", newIsAuto ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.valve.value != newValue) {
                currentCommands.valve.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Valve value: %s\n", newValue ? "true" : "false");
            }
        }
    }
    else if (dataPath == "/cooler") {
        // Update cooler command
        if (!patchData["isAuto"].isNull()) {
            bool newIsAuto = patchData["isAuto"].as<bool>();
            if (currentCommands.cooler.isAuto != newIsAuto) {
                currentCommands.cooler.isAuto = newIsAuto;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Cooler isAuto: %s\n", newIsAuto ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.cooler.value != newValue) {
                currentCommands.cooler.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Cooler value: %s\n", newValue ? "true" : "false");
            }
        }
    }
    else if (dataPath == "/heater") {
        // Update heater command
        if (!patchData["isAuto"].isNull()) {
            bool newIsAuto = patchData["isAuto"].as<bool>();
            if (currentCommands.heater.isAuto != newIsAuto) {
                currentCommands.heater.isAuto = newIsAuto;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Heater isAuto: %s\n", newIsAuto ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.heater.value != newValue) {
                currentCommands.heater.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] Heater value: %s\n", newValue ? "true" : "false");
            }
        }
    }
    else if (dataPath == "/phDosing") {
        if (!patchData["phDosingEnabled"].isNull()) {
            bool enabled = patchData["phDosingEnabled"].as<bool>();
            if (currentCommands.phDosing.phDosingEnabled != enabled) {
                currentCommands.phDosing.phDosingEnabled = enabled;
                hasChanges = true;
                Serial.printf("[RTDB Stream] pH dosing enabled: %s\n", enabled ? "true" : "false");
            }
        }
        if (!patchData["value"].isNull()) {
            bool newValue = patchData["value"].as<bool>();
            if (currentCommands.phDosing.value != newValue) {
                currentCommands.phDosing.value = newValue;
                hasChanges = true;
                Serial.printf("[RTDB Stream] pH dosing value: %s\n", newValue ? "true" : "false");
            }
        }
    }

    
    else {
        Serial.printf("[RTDB Stream] Unknown path: %s\n", dataPath.c_str());
    }
}

// Stream callback function to handle real-time command updates
void onRTDBStream(AsyncResult &result) {
    if (result.isError()) {
        Serial.printf("Error task: %s, msg: %s, code: %d\n",
                      result.uid().c_str(),
                      result.error().message().c_str(),
                      result.error().code());

        if (result.error().code() == -106) {
            Serial.println("[RTDB Stream] Auth lost (code -106). Will re-auth and restart stream.");
        }

        streamConnected = false;
        // Require resync when stream errors
        initialCommandsSynced = false;
        needResyncOnReconnect = true;
    }

    if (!result.available()) return;

    Serial.println("[RTDB Stream] Data updated");

    if (!streamConnected) {
        streamConnected = true;
        Serial.println("[RTDB Stream] ✅ Stream connected");
    }

    String raw = result.c_str();
    Serial.printf("[RTDB Stream] Received data: %s\n", raw.c_str());

    String data;
    if (!extractSSEJson(raw, data)) {
        Serial.println("[RTDB Stream] Unable to extract JSON from SSE payload; ignoring");
        return;
    }

    if (data.isEmpty() || data == "null") {
        Serial.println("[RTDB Stream] Empty or null event, ignoring...");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err) {
        Serial.printf("[RTDB Stream] JSON parse error: %s\n", err.c_str());
        return;
    }

    extern Commands currentCommands;
    bool hasChanges = false;
    bool autoTriggered = false; // <—— track if AUTO got re-enabled

    auto applyManualIfNeeded = [&]() {
        if (!currentCommands.fan.isAuto) {
            control_fan(currentCommands.fan.value);
            Serial.printf("[RTDB Stream] Manual FAN applied: %s\n", currentCommands.fan.value ? "ON" : "OFF");
        }
        if (!currentCommands.light.isAuto) {
            control_light(currentCommands.light.value);
            Serial.printf("[RTDB Stream] Manual LIGHT applied: %s\n", currentCommands.light.value ? "ON" : "OFF");
        }
        if (!currentCommands.pump.isAuto) {
            control_pump(currentCommands.pump.value);
            Serial.printf("[RTDB Stream] Manual PUMP applied: %s\n", currentCommands.pump.value ? "ON" : "OFF");
        }
        if (!currentCommands.valve.isAuto) {
            control_valve(currentCommands.valve.value);
            Serial.printf("[RTDB Stream] Manual VALVE applied: %s\n", currentCommands.valve.value ? "ON" : "OFF");
        }
        if (!currentCommands.cooler.isAuto) {
            control_cooler(currentCommands.cooler.value);
            Serial.printf("[RTDB Stream] Manual WATER_COOLER applied: %s\n", currentCommands.cooler.value ? "ON" : "OFF");
        }
        if (!currentCommands.heater.isAuto) {
            control_heater(currentCommands.heater.value);
            Serial.printf("[RTDB Stream] Manual WATER_HEATER applied: %s\n", currentCommands.heater.value ? "ON" : "OFF");
        }
    };

    // PATCH or SNAPSHOT handling (same as your original)
    if (!doc["path"].isNull()) {
        String changedPath = doc["path"].as<String>();
        JsonVariant changedData = doc["data"];

        Serial.printf("[RTDB Stream] Patch path: %s\n", changedPath.c_str());
        if (changedData.isNull()) return;

        auto handleActuator = [&](const char* name) {
            if (changedPath.startsWith(String("/") + name)) {
                bool oldAuto = false;
                bool newAuto = false;

                if (strcmp(name, "fan") == 0) oldAuto = currentCommands.fan.isAuto;
                else if (strcmp(name, "light") == 0) oldAuto = currentCommands.light.isAuto;
                else if (strcmp(name, "pump") == 0) oldAuto = currentCommands.pump.isAuto;
                else if (strcmp(name, "valve") == 0) oldAuto = currentCommands.valve.isAuto;
                else if (strcmp(name, "cooler") == 0) oldAuto = currentCommands.cooler.isAuto;
                else if (strcmp(name, "heater") == 0) oldAuto = currentCommands.heater.isAuto;

                // Apply patch changes
                JsonDocument obj; 
                obj.set(changedData);
                processPatchEvent(String("/") + name, obj, currentCommands, hasChanges);

                // Detect if toggled back to AUTO mode
                if (!obj["isAuto"].isNull()) {
                    newAuto = obj["isAuto"].as<bool>();
                    if (!oldAuto && newAuto) {
                        Serial.printf("[RTDB Stream] %s switched to AUTO — triggering rule evaluation\n", name);
                        autoTriggered = true;
                        extern volatile bool commandsChangedViaStream;
                        commandsChangedViaStream = true;
                    }
                }
            }
        };


        handleActuator("fan");
        handleActuator("light");
        handleActuator("pump");
        handleActuator("valve");
        handleActuator("cooler");
        handleActuator("heater");
        if (changedPath.startsWith("/phDosing")) {
            bool wasEnabled = currentCommands.phDosing.phDosingEnabled;
            JsonDocument obj;
            obj.set(changedData);
            processPatchEvent("/phDosing", obj, currentCommands, hasChanges);
            if (!obj["phDosingEnabled"].isNull()) {
                bool enabledNow = obj["phDosingEnabled"].as<bool>();
                if (!wasEnabled && enabledNow) {
                    Serial.println("[RTDB Stream] pH dosing switched ON — evaluating automation");
                    autoTriggered = true;
                    extern volatile bool commandsChangedViaStream;
                    commandsChangedViaStream = true;
                }
            }
        }

        applyManualIfNeeded();
    } else {
        // Snapshot handling
        if (!doc["fan"].isNull()) {
            JsonDocument obj; obj.set(doc["fan"]);
            processPatchEvent("/fan", obj, currentCommands, hasChanges);
        }
        if (!doc["light"].isNull()) {
            JsonDocument obj; obj.set(doc["light"]);
            processPatchEvent("/light", obj, currentCommands, hasChanges);
        }
        if (!doc["pump"].isNull()) {
            JsonDocument obj; obj.set(doc["pump"]);
            processPatchEvent("/pump", obj, currentCommands, hasChanges);
        }
        if (!doc["valve"].isNull()) {
            JsonDocument obj; obj.set(doc["valve"]);
            processPatchEvent("/valve", obj, currentCommands, hasChanges);
        }
        if (!doc["cooler"].isNull()) {
            JsonDocument obj; obj.set(doc["cooler"]);
            processPatchEvent("/cooler", obj, currentCommands, hasChanges);
        }
        if (!doc["heater"].isNull()) {
            JsonDocument obj; obj.set(doc["heater"]);
            processPatchEvent("/heater", obj, currentCommands, hasChanges);
        }
        if (!doc["phDosing"].isNull()) {
            JsonDocument obj; obj.set(doc["phDosing"]);
            processPatchEvent("/phDosing", obj, currentCommands, hasChanges);
        }
        applyManualIfNeeded();
    }

    if (hasChanges) {
        Serial.println("[RTDB Stream] Commands updated successfully");
        extern volatile bool commandsChangedViaStream;
        commandsChangedViaStream = true;

        extern unsigned long lastStreamUpdate;
        lastStreamUpdate = millis();
    }

    //Trigger rule engine immediately when switching back to AUTO
    if (autoTriggered && initialCommandsSynced) {
        Serial.println("[RTDB Stream] AUTO mode re-enabled — evaluating rule engine now...");
        evaluateRules(true);

        // Force sync relay states after rule evaluation
        extern RealTimeData current;   // make sure current holds the latest relay states
        extern Commands currentCommands;
        Serial.println("[RTDB Stream] Forcing immediate sync after AUTO re-enable...");

        // Directly reflect current actuator GPIO states (true source of truth)
        extern ActuatorState actuators;

        RealTimeData snapshot = current;  // start from existing data
        snapshot.relayStates.fan       = actuators.fan;
        snapshot.relayStates.light     = actuators.light;
        snapshot.relayStates.waterPump = actuators.pump;
        snapshot.relayStates.valve     = actuators.valve;
        snapshot.relayStates.cooler= actuators.cooler;
        snapshot.relayStates.heater= actuators.heater;

        syncRelayState(snapshot, currentCommands);
        Serial.println("[RTDB Stream] Immediate sync completed.");
    }
}

// Initialize and start the Firebase RTDB stream (token-aware)
void startFirebaseStream() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[RTDB Stream] ❌ WiFi not connected, cannot start stream");
        return;
    }

    if (!tokenValid) {
        Serial.println("[RTDB Stream] ⚠️ Token invalid, delaying stream start until re-auth");
        return;
    }

    Serial.println("[RTDB Stream] Starting Firebase RTDB stream...");

    // One-time app initialization
    if (!firebaseAppInitialized) {
        ssl_client.setInsecure();
        aClient.setNetwork(ssl_client, getNetwork(defaultNet));

        UserAuth user_auth(FIREBASE_API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
        initializeApp(aClient, app, getAuth(user_auth), onRTDBStream, "authTask");

        firebaseAppInitialized = true;
        Serial.println("[RTDB Stream] FirebaseApp initialization requested");
    }

    // Wait for Firebase app to finish auth handshake
    if (!app.ready()) {
        Serial.println("[RTDB Stream] ⏳ App not ready yet (auth in progress). Will try again later.");
        lastStreamReconnectAttempt = millis();
        return;
    }

    // Mark Firebase as ready once the app is live
    firebaseReady = true;

    // Fetch latest /commands before subscribing to avoid overwriting manual changes
    if (needResyncOnReconnect || !initialCommandsSynced) {
        Serial.println("[RTDB] Fetching /commands before starting stream...");
        bool manualApplied = fetchControlCommands();
        if (manualApplied) Serial.println("[RTDB] Manual states applied from initial fetch");
        initialCommandsSynced = true;
        needResyncOnReconnect = false;
    }

    // Configure Realtime Database
    app.getApp<RealtimeDatabase>(Database);
    Database.url("https://aquabell-cap2025-default-rtdb.asia-southeast1.firebasedatabase.app");

    // Start stream listener
    String path = "/commands/" DEVICE_ID;
    Database.get(aClient, path, onRTDBStream, "streamTask");

    Serial.println("[RTDB Stream] ✅ Stream start requested");
    streamConnected = false;
    lastStreamReconnectAttempt = millis();
    databaseConfigured = true;
}

// Robust stream loop handling (WiFi + token + reauth aware)
void handleFirebaseStream() {
    unsigned long now = millis();

    // --- Handle lost WiFi ---
    if (WiFi.status() != WL_CONNECTED) {
        if (streamConnected) {
            Serial.println("[RTDB Stream] ⚠️ WiFi lost, marking stream disconnected");
            streamConnected = false;
        }
        initialCommandsSynced = false;
        needResyncOnReconnect = true;
        return;
    }

    // --- Handle expired or invalid token ---
    if (!tokenValid) {
        static unsigned long lastLog = 0;
        if (now - lastLog > 5000) { // avoid spamming serial
            Serial.println("[RTDB Stream] ⚠️ Token invalid — holding stream reconnection");
            lastLog = now;
        }
        safeTokenRefresh();
        return;
    }

    // --- Drive Firebase app state machine ---
    app.loop();

    // --- Attempt reconnect if stream lost ---
    if (!streamConnected) {
        if (now - lastStreamReconnectAttempt >= STREAM_RECONNECT_INTERVAL) {
            Serial.println("[RTDB Stream] 🔄 Attempting to (re)start stream...");
            startFirebaseStream();
        }
    }

    // --- Drive DB state when configured ---
    if (databaseConfigured) {
        Database.loop();
    }
}

//NOTE: isStreamConnected is not used, safe to delete
// Check if the Firebase stream is currently connected
bool isStreamConnected() {
    return streamConnected;
}

// Expose readiness flags
bool isFirebaseReady() {
    return firebaseReady;
}

bool isInitialCommandsSynced() {
    return initialCommandsSynced;
}