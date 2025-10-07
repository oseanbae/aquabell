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
#include "firestore_client.h"
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

void firebaseSignIn() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.begin(client, "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" FIREBASE_API_KEY);
    https.addHeader("Content-Type", "application/json");

    String payload = "{\"email\": \"" USER_EMAIL "\", \"password\": \"" USER_PASSWORD "\", \"returnSecureToken\": true}";

    int httpResponseCode = https.POST(payload);

    if (httpResponseCode == 200) {
        String response = https.getString();
        JsonDocument doc;
        deserializeJson(doc, response);

        idToken = doc["idToken"].as<String>();
        refreshToken = doc["refreshToken"].as<String>();
        int expiresIn = doc["expiresIn"].as<int>();
        tokenExpiryTime = millis() + (expiresIn - 180) * 1000UL; // Refresh 3 min before expiry

        Serial.println("[Auth] Signed in successfully.");
        firebaseReady = true;
    } else {
        Serial.print("[Auth] Failed to sign in: ");
        Serial.println(https.errorToString(httpResponseCode));
    }

    https.end();
}

void refreshIdToken() {
    if (refreshToken == "") {
        Serial.println("[Auth] No refresh token available. Re-signing in.");
        firebaseSignIn();
        return;
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
        tokenExpiryTime = millis() + (expiresIn - 60) * 1000UL; // Refresh 1 min before expiry

        Serial.println("[Auth] Token refreshed successfully.");
    } else {
        Serial.print("[Auth] Failed to refresh token: ");
        Serial.println(https.errorToString(httpResponseCode));
    }

    https.end();
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

void pushToFirestoreLive(const RealTimeData &data) {
    // Simplified version - minimal error checking
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Skip if token is expired - don't try to refresh to avoid crashes
    if (millis() > tokenExpiryTime) {
        Serial.println("[FIRESTORE] Token expired, skipping update");
        return;
    }

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/live_data/" DEVICE_ID;

    JsonDocument doc; // compact live payload
    JsonObject fields = doc["fields"].to<JsonObject>();

    // Validate data before sending to Firestore
    if (!isnan(data.waterTemp)) fields["waterTemp"]["doubleValue"] = data.waterTemp;
    if (!isnan(data.pH)) fields["pH"]["doubleValue"] = data.pH;
    if (!isnan(data.dissolvedOxygen)) fields["dissolvedOxygen"]["doubleValue"] = data.dissolvedOxygen;
    if (!isnan(data.turbidityNTU)) fields["turbidityNTU"]["doubleValue"] = data.turbidityNTU;
    if (!isnan(data.airTemp)) fields["airTemp"]["doubleValue"] = data.airTemp;
    if (!isnan(data.airHumidity)) fields["airHumidity"]["doubleValue"] = data.airHumidity;
    fields["floatTriggered"]["booleanValue"] = data.floatTriggered;
    // 🔑 Force update trigger: add timestamp
    fields["timestamp"]["integerValue"] = time(nullptr);

    String payload;
    serializeJson(doc, payload);

    // Attempt the PATCH request with proper error handling
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000); // 10 second timeout
    
    HTTPClient https;
    https.setTimeout(10000); // 10 second timeout
    https.begin(client, url);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = https.sendRequest("PATCH", payload);
    Serial.printf("[FIRESTORE] pushToFirestoreLive HTTP response: %d\n", httpResponseCode);

    if (httpResponseCode == 200) {
        Serial.println("[FIRESTORE] Live data updated successfully");
    } else {
        String response = https.getString();
        Serial.printf("[FIRESTORE] Update failed: %s\n", https.errorToString(httpResponseCode).c_str());
        Serial.printf("[FIRESTORE] Response body: %s\n", response.c_str());
        
        // Initialize non-blocking retry instead of blocking delay
        Serial.println("[FIRESTORE] Initializing non-blocking retry for live data push");
        initRetryState(url, payload, false); // false indicates this is not a relay sync operation
    }
    
    // Always end the connection to free memory
    https.end();
}

void pushBatchLogToFirestore(RealTimeData *buffer, int size, time_t timestamp) {
    // Check and refresh token before Firestore call
    if (millis() > tokenExpiryTime) {
        Serial.println("[Firestore] Token expired, refreshing before pushBatchLogToFirestore");
        refreshIdToken();
    }

    if (size <= 0) return;

    // Compute 5-min averages over the buffer
    double sumWater = 0, sumPH = 0, sumDO = 0, sumNTU = 0, sumAirT = 0, sumAirH = 0;
    int count = 0;
    for (int i = 0; i < size; i++) {
        sumWater += buffer[i].waterTemp;
        sumPH += buffer[i].pH;
        sumDO += buffer[i].dissolvedOxygen;
        sumNTU += buffer[i].turbidityNTU;
        sumAirT += buffer[i].airTemp;
        sumAirH += buffer[i].airHumidity;
        count++;
    }

    float avgWater = count ? (float)(sumWater / count) : 0;
    float avgPH = count ? (float)(sumPH / count) : 0;
    float avgDO = count ? (float)(sumDO / count) : 0;
    float avgNTU = count ? (float)(sumNTU / count) : 0;
    float avgAirT = count ? (float)(sumAirT / count) : 0;
    float avgAirH = count ? (float)(sumAirH / count) : 0;

    // Format date string from timestamp
    struct tm timeinfo;
    char dateStr[11];  // YYYY-MM-DD + null terminator
    if (localtime_r(&timestamp, &timeinfo)) {
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    } else {
        strcpy(dateStr, "1970-01-01"); // Fallback
    }
    
    String timestampStr = String(timestamp);

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/sensor_logs/" + String(dateStr) + "_" + timestampStr;

    JsonDocument doc; // compact averaged payload
    JsonObject fields = doc["fields"].to<JsonObject>();
    fields["timestamp"]["integerValue"] = (long long)timestamp;
    fields["avgWaterTemp"]["doubleValue"] = avgWater;
    fields["avgPH"]["doubleValue"] = avgPH;
    fields["avgDO"]["doubleValue"] = avgDO;
    fields["avgTurbidityNTU"]["doubleValue"] = avgNTU;
    fields["avgAirTemp"]["doubleValue"] = avgAirT;
    fields["avgAirHumidity"]["doubleValue"] = avgAirH;
    fields["count"]["integerValue"] = count;

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
        Serial1.println("[Firestore] Batch log updated.");
    } else {
        Serial.printf("[Firestore] Batch failed: %s\n", https.errorToString(httpResponseCode).c_str());
        delay(5000);
        httpResponseCode = https.sendRequest("PATCH", payload);
        if (httpResponseCode == 200) Serial.println("[Firestore] ✅ Batch retry successful");
    }

    https.end();
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

    // Check each actuator individually
    const char* actuators[] = {"fan", "light", "pump", "valve"};
    void (*controlFunctions[])(bool) = {control_fan, control_light, control_pump, control_valve};

    for (int i = 0; i < 4; i++) {
        const char* actuator = actuators[i];
        
        if (!doc[actuator].isNull()) {
            JsonObject actuatorData = doc[actuator].as<JsonObject>();
            if (!actuatorData.isNull()) {
                bool isAuto = actuatorData["isAuto"].as<bool>();
                bool value = actuatorData["value"].as<bool>();
                
                Serial.printf("[Control] %s isAuto=%s value=%s\n", 
                             actuator, isAuto ? "true" : "false", value ? "true" : "false");
                
                if (!isAuto) { // Manual mode when isAuto is false
                    controlFunctions[i](value);
                    anyManualControl = true;
                    Serial.printf("[Control] Applied manual control for %s\n", actuator);
                }
            }
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

    if (commands.fan.isAuto) {
        doc["fan/value"] = data.relayStates.fan;
        Serial.println("[RTDB] Syncing FAN state (AUTO mode)");
    } else Serial.println("[RTDB] Skipping FAN (manual mode)");

    if (commands.light.isAuto) {
        doc["light/value"] = data.relayStates.light;
        Serial.println("[RTDB] Syncing LIGHT state (AUTO mode)");
    } else Serial.println("[RTDB] Skipping LIGHT (manual mode)");

    if (commands.pump.isAuto) {
        doc["pump/value"] = data.relayStates.waterPump;
        Serial.println("[RTDB] Syncing PUMP state (AUTO mode)");
    } else Serial.println("[RTDB] Skipping PUMP (manual mode)");

    if (commands.valve.isAuto) {
        doc["valve/value"] = data.relayStates.valve;
        Serial.println("[RTDB] Syncing VALVE state (AUTO mode)");
    } else Serial.println("[RTDB] Skipping VALVE (manual mode)");

    if (doc.size() == 0) {
        Serial.println("[RTDB] No AUTO actuators to sync — skipping update");
        return;
    }

    String payload;
    serializeJson(doc, payload);
    Serial.printf("[RTDB] Sync payload: %s\n", payload.c_str());

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, url);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = https.sendRequest("PATCH", payload);
    Serial.printf("[RTDB] HTTP Response: %d\n", httpResponseCode);

    if (httpResponseCode == 200) {
        Serial.println("[RTDB] Relay sync successful ✅");
    } else {
        String response = https.getString();
        Serial.printf("[RTDB] Relay sync failed: %s\n", https.errorToString(httpResponseCode).c_str());
        Serial.printf("[RTDB] Response: %s\n", response.c_str());
        Serial.println("[RTDB] Scheduling non-blocking retry for relay sync");
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
    else {
        Serial.printf("[RTDB Stream] Unknown path: %s\n", dataPath.c_str());
    }
}

// Stream callback function to handle real-time command updates
void onRTDBStream(AsyncResult &result) {
    if (result.isEvent()) {
        Serial.printf("Event task: %s, msg: %s, code: %d\n",
                      result.uid().c_str(),
                      result.appEvent().message().c_str(),
                      result.appEvent().code());
    }

    if (result.isError()) {
        Serial.printf("Error task: %s, msg: %s, code: %d\n",
                      result.uid().c_str(),
                      result.error().message().c_str(),
                      result.error().code());
        // -106 unauthenticate → force reconnect flow
        if (result.error().code() == -106) {
            Serial.println("[RTDB Stream] Auth lost (code -106). Will re-auth and restart stream.");
        }
        streamConnected = false;
        // Require resync when stream errors
        initialCommandsSynced = false;
        needResyncOnReconnect = true;
    }

    if (result.available()) {
        Serial.println("[RTDB Stream] Data updated");
        // Mark connected on first data/event
        if (!streamConnected) {
            streamConnected = true;
            Serial.println("[RTDB Stream] ✅ Stream connected");
        }

        // Get the raw data from the stream
        String data = result.c_str();
        Serial.printf("[RTDB Stream] Received data: %s\n", data.c_str());

        // External Commands struct to update (passed by reference)
        extern Commands currentCommands;
        bool hasChanges = false;

        if (data.isEmpty() || data == "null") {
            Serial.println("[RTDB Stream] Empty or null event, ignoring...");
            return;
        }

        // Parse the JSON data
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data);
        if (err) {
            Serial.printf("[RTDB Stream] JSON parse error: %s\n", err.c_str());
            return;
        }

        // Two possible payload shapes:
        // 1) Full snapshot: {"fan": {..}, "light": {..}, ...}
        // 2) Patch event:   {"path": "/pump/isAuto", "data": false}

        auto applyManualIfNeeded = [&]() {
            // When isAuto == false, immediately apply manual value to relay
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
        };

        if (!doc["path"].isNull()) {
            // Patch payload
            String changedPath = doc["path"].as<String>();
            JsonVariant changedData = doc["data"]; // may be null on deletes
            Serial.printf("[RTDB Stream] Patch path: %s\n", changedPath.c_str());

            // If data is explicitly null (delete), ignore to avoid resetting others
            if (changedData.isNull()) {
                Serial.println("[RTDB Stream] Patch with null data; ignoring");
                return;
            }

            // Normalize known component paths
            if (changedPath.startsWith("/fan")) {
                if (changedPath.endsWith("/isAuto")) {
                    bool newIsAuto = changedData.as<bool>();
                    if (currentCommands.fan.isAuto != newIsAuto) {
                        currentCommands.fan.isAuto = newIsAuto;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Fan isAuto: %s\n", newIsAuto ? "true" : "false");
                    }
                } else if (changedPath.endsWith("/value")) {
                    bool newValue = changedData.as<bool>();
                    if (currentCommands.fan.value != newValue) {
                        currentCommands.fan.value = newValue;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Fan value: %s\n", newValue ? "true" : "false");
                    }
                } else if (changedData.is<JsonObject>()) {
                    JsonDocument obj;
                    obj.set(changedData);
                    processPatchEvent("/fan", obj, currentCommands, hasChanges);
                }
            } else if (changedPath.startsWith("/light")) {
                if (changedPath.endsWith("/isAuto")) {
                    bool newIsAuto = changedData.as<bool>();
                    if (currentCommands.light.isAuto != newIsAuto) {
                        currentCommands.light.isAuto = newIsAuto;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Light isAuto: %s\n", newIsAuto ? "true" : "false");
                    }
                } else if (changedPath.endsWith("/value")) {
                    bool newValue = changedData.as<bool>();
                    if (currentCommands.light.value != newValue) {
                        currentCommands.light.value = newValue;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Light value: %s\n", newValue ? "true" : "false");
                    }
                } else if (changedData.is<JsonObject>()) {
                    JsonDocument obj;
                    obj.set(changedData);
                    processPatchEvent("/light", obj, currentCommands, hasChanges);
                }
            } else if (changedPath.startsWith("/pump")) {
                if (changedPath.endsWith("/isAuto")) {
                    bool newIsAuto = changedData.as<bool>();
                    if (currentCommands.pump.isAuto != newIsAuto) {
                        currentCommands.pump.isAuto = newIsAuto;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Pump isAuto: %s\n", newIsAuto ? "true" : "false");
                    }
                } else if (changedPath.endsWith("/value")) {
                    bool newValue = changedData.as<bool>();
                    if (currentCommands.pump.value != newValue) {
                        currentCommands.pump.value = newValue;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Pump value: %s\n", newValue ? "true" : "false");
                    }
                } else if (changedData.is<JsonObject>()) {
                    JsonDocument obj;
                    obj.set(changedData);
                    processPatchEvent("/pump", obj, currentCommands, hasChanges);
                }
            } else if (changedPath.startsWith("/valve")) {
                if (changedPath.endsWith("/isAuto")) {
                    bool newIsAuto = changedData.as<bool>();
                    if (currentCommands.valve.isAuto != newIsAuto) {
                        currentCommands.valve.isAuto = newIsAuto;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Valve isAuto: %s\n", newIsAuto ? "true" : "false");
                    }
                } else if (changedPath.endsWith("/value")) {
                    bool newValue = changedData.as<bool>();
                    if (currentCommands.valve.value != newValue) {
                        currentCommands.valve.value = newValue;
                        hasChanges = true;
                        Serial.printf("[RTDB Stream] Valve value: %s\n", newValue ? "true" : "false");
                    }
                } else if (changedData.is<JsonObject>()) {
                    JsonDocument obj;
                    obj.set(changedData);
                    processPatchEvent("/valve", obj, currentCommands, hasChanges);
                }
            } else {
                Serial.printf("[RTDB Stream] Patch path not recognized: %s\n", changedPath.c_str());
            }

            // Apply manual overrides immediately if any
            applyManualIfNeeded();

        } else {
            // Full snapshot payload
            if (!doc["fan"].isNull()) {
                Serial.println("[RTDB Stream] Processing fan update");
                JsonDocument fanData;
                fanData.set(doc["fan"]);
                processPatchEvent("/fan", fanData, currentCommands, hasChanges);
            }

            if (!doc["light"].isNull()) {
                Serial.println("[RTDB Stream] Processing light update");
                JsonDocument lightData;
                lightData.set(doc["light"]);
                processPatchEvent("/light", lightData, currentCommands, hasChanges);
            }

            if (!doc["pump"].isNull()) {
                Serial.println("[RTDB Stream] Processing pump update");
                JsonDocument pumpData;
                pumpData.set(doc["pump"]);
                processPatchEvent("/pump", pumpData, currentCommands, hasChanges);
            }

            if (!doc["valve"].isNull()) {
                Serial.println("[RTDB Stream] Processing valve update");
                JsonDocument valveData;
                valveData.set(doc["valve"]);
                processPatchEvent("/valve", valveData, currentCommands, hasChanges);
            }

            // Apply manual overrides immediately if snapshot puts any device to manual
            applyManualIfNeeded();
            // Mark initial sync completed on first valid snapshot
            if (!initialCommandsSynced) {
                initialCommandsSynced = true;
                needResyncOnReconnect = false;
                Serial.println("[RTDB Stream] ✅ Initial commands synced via snapshot");
            }
        }

        if (hasChanges) {
            Serial.println("[RTDB Stream] Commands updated successfully");
            // Set flag to trigger rule application in main loop
            extern volatile bool commandsChangedViaStream;
            commandsChangedViaStream = true;
        }
    }
}


// Initialize and start the Firebase RTDB stream
void startFirebaseStream() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[RTDB Stream] WiFi not connected, cannot start stream");
        return;
    }

    Serial.println("[RTDB Stream] Starting Firebase RTDB stream...");

    // One-time app initialization (non-blocking); App handles auth internally
    if (!firebaseAppInitialized) {
        // Bind WiFi SSL client and default network to AsyncClient before auth
        ssl_client.setInsecure();
        aClient.setNetwork(ssl_client, getNetwork(defaultNet));

        UserAuth user_auth(FIREBASE_API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
        initializeApp(aClient, app, getAuth(user_auth), onRTDBStream, "authTask");
        firebaseAppInitialized = true;
        Serial.println("[RTDB Stream] FirebaseApp initialization requested");
    }

    // Only start stream when app is ready (authenticated)
    if (!app.ready()) {
        Serial.println("[RTDB Stream] App not ready yet (auth in progress). Will try again later.");
        lastStreamReconnectAttempt = millis();
        return;
    }

    // Mark firebase ready and fetch commands once before starting stream
    firebaseReady = true;

    // On fresh start or after reconnect, fetch latest /commands to avoid overwriting manual states
    if (needResyncOnReconnect || !initialCommandsSynced) {
        Serial.println("[RTDB] Fetching /commands before starting stream...");
        bool manualApplied = fetchControlCommands();
        if (manualApplied) {
            Serial.println("[RTDB] Manual states applied from initial fetch");
        }
        initialCommandsSynced = true; // consider synced after successful GET (even if no manual)
        needResyncOnReconnect = false;
    }

    // Get RealtimeDatabase instance and set base URL once app is available
    app.getApp<RealtimeDatabase>(Database);
    Database.url("https://aquabell-cap2025-default-rtdb.asia-southeast1.firebasedatabase.app");

    // Start listening to the commands path via callback-based stream
    String path = "/commands/" DEVICE_ID;
    Database.get(aClient, path, onRTDBStream, "streamTask");

    Serial.println("[RTDB Stream] Stream start requested");
    streamConnected = false; // will flip true on first available/event
    lastStreamReconnectAttempt = millis();
    databaseConfigured = true;
}

// Handle Firebase stream - must be called in loop()
void handleFirebaseStream() {
    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        if (streamConnected) {
            Serial.println("[RTDB Stream] WiFi disconnected, marking stream as disconnected");
            streamConnected = false;
        }
        // WiFi lost; require resync on reconnect and block RTDB writes
        initialCommandsSynced = false;
        needResyncOnReconnect = true;
        return;
    }
    
    // Ensure auth/initialization progresses
    app.loop();

    if (!streamConnected) {
        unsigned long now = millis();
        if (now - lastStreamReconnectAttempt >= STREAM_RECONNECT_INTERVAL) {
            Serial.println("[RTDB Stream] Attempting to (re)start stream...");
            startFirebaseStream();
        }
        // Drive DB state only after it's configured
        if (databaseConfigured) {
            Database.loop();
        }
        return;
    }

    // Process database loop when connected
    if (databaseConfigured) {
        Database.loop();
    }
}

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