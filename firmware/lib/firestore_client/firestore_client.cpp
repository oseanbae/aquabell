#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor_data.h"
#include "RTClib.h"
#include "relay_control.h"

#define FIREBASE_PROJECT_ID "aquabell-cap2025"
#define DEVICE_ID "aquabell_esp32"
#define FIREBASE_API_KEY "AIzaSyCOPHwBDym3PNQFXQX9MqiojBSO4mnnIb4"
#define USER_EMAIL "esp32@test.com"
#define USER_PASSWORD "cap413"

String idToken = "";
String refreshToken = "";
unsigned long tokenExpiryTime = 0;

// RTC instance is defined in main.cpp, we'll use it via parameter

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
        tokenExpiryTime = millis() + (expiresIn - 60) * 1000UL; // Refresh 1 min before expiry

        Serial.println("[Auth] Signed in successfully.");
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

void pushToFirestoreLive(const RealTimeData &data) {
    if (millis() > tokenExpiryTime) refreshIdToken();

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/live_data/" DEVICE_ID;

    StaticJsonDocument<768> doc; // compact live payload
    JsonObject fields = doc["fields"].to<JsonObject>();

    fields["waterTemp"]["doubleValue"] = data.waterTemp;
    fields["pH"]["doubleValue"] = data.pH;
    fields["dissolvedOxygen"]["doubleValue"] = data.dissolvedOxygen;
    fields["turbidityNTU"]["doubleValue"] = data.turbidityNTU;
    fields["airTemp"]["doubleValue"] = data.airTemp;
    fields["airHumidity"]["doubleValue"] = data.airHumidity;
    fields["floatTriggered"]["booleanValue"] = data.floatTriggered;

    // Relay States as Map
    JsonObject relayMap = fields["relayStates"]["mapValue"]["fields"].to<JsonObject>();
    relayMap["fan"]["booleanValue"] = data.relayStates.fan;
    relayMap["light"]["booleanValue"] = data.relayStates.light;
    relayMap["waterPump"]["booleanValue"] = data.relayStates.waterPump;
    relayMap["valve"]["booleanValue"] = data.relayStates.valve;

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
        Serial.println("[Firestore] Live data updated.");
    } else {
        Serial.print("[Firestore] Live data update failed: ");
        Serial.println(https.errorToString(httpResponseCode));
        delay(5000);
        httpResponseCode = https.sendRequest("PATCH", payload);
        if (httpResponseCode == 200) Serial.println("[Firestore] Live data updated on retry.");
    }

    https.end();
}


void pushBatchLogToFirestore(RealTimeData *buffer, int size, const DateTime& now) {
    if (millis() > tokenExpiryTime) refreshIdToken();

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

    char dateStr[11];  // YYYY-MM-DD
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", now.year(), now.month(), now.day());
    String timestamp = String(now.unixtime());

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/sensor_logs/" + String(dateStr) + "_" + timestamp;

    StaticJsonDocument<512> doc; // compact averaged payload
    JsonObject fields = doc["fields"].to<JsonObject>();
    fields["timestamp"]["integerValue"] = (long long)now.unixtime();
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
        Serial.println("[Firestore] Batch average log pushed.");
    } else {
        Serial.print("[Firestore] Batch avg push failed: ");
        Serial.println(https.errorToString(httpResponseCode));
        delay(5000);
        httpResponseCode = https.sendRequest("PATCH", payload);
        if (httpResponseCode == 200) Serial.println("[Firestore] Batch average log pushed on retry.");
    }

    https.end();
}

bool fetchControlCommands() {
    if (millis() > tokenExpiryTime) refreshIdToken();

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/control_commands/" DEVICE_ID;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, url);
    https.addHeader("Authorization", "Bearer " + idToken);

    int httpResponseCode = https.GET();
    if (httpResponseCode != 200) {
        Serial.print("[Firestore] Fetch control commands failed: ");
        Serial.println(https.errorToString(httpResponseCode));
        delay(5000);
        httpResponseCode = https.GET();
        if (httpResponseCode != 200) {
            https.end();
            return false;
        }
    }

    String response = https.getString();
    https.end();

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.print("[Firestore] JSON parse error: ");
        Serial.println(err.f_str());
        return false;
    }

    JsonObject fields = doc["fields"].as<JsonObject>();
    if (fields.isNull()) return false;

    String mode = fields["mode"]["stringValue"].as<String>();
    Serial.println(String("[Control] mode=") + mode);

    if (mode == "manual") {
        JsonObject manual = fields["manualRelayStates"]["mapValue"]["fields"].as<JsonObject>();
        if (!manual.isNull()) {
            if (!manual["fan"].isNull())    control_fan(manual["fan"]["booleanValue"].as<bool>());
            if (!manual["light"].isNull())  control_light(manual["light"]["booleanValue"].as<bool>());
            if (!manual["pump"].isNull())   control_pump(manual["pump"]["booleanValue"].as<bool>());
            if (!manual["valve"].isNull())  control_valve(manual["valve"]["booleanValue"].as<bool>());
            Serial.println("[Control] Applied manual relay states.");
        }
        return true; // manual mode handled
    }

    // auto mode: nothing to apply here; rule engine should take over
    return false;
}

void syncRelayState(const RealTimeData &data) {
    if (millis() > tokenExpiryTime) refreshIdToken();

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/live_data/" DEVICE_ID;

    StaticJsonDocument<384> doc;
    JsonObject fields = doc["fields"].to<JsonObject>();
    JsonObject relayMap = fields["relayStates"]["mapValue"]["fields"].to<JsonObject>();
    relayMap["fan"]["booleanValue"] = data.relayStates.fan;
    relayMap["light"]["booleanValue"] = data.relayStates.light;
    relayMap["waterPump"]["booleanValue"] = data.relayStates.waterPump;
    relayMap["valve"]["booleanValue"] = data.relayStates.valve;

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
        Serial.println("[Firestore] Relay states synced.");
    } else {
        Serial.print("[Firestore] Relay sync failed: ");
        Serial.println(https.errorToString(httpResponseCode));
        delay(5000);
        httpResponseCode = https.sendRequest("PATCH", payload);
        if (httpResponseCode == 200) Serial.println("[Firestore] Relay states synced on retry.");
    }

    https.end();
}