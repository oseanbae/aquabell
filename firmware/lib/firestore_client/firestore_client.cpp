#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor_data.h"
#include "RTClib.h"

#define FIREBASE_PROJECT_ID "aquabell-cap2025"
#define DEVICE_ID "aquabell_esp32"
#define FIREBASE_API_KEY "AIzaSyCOPHwBDym3PNQFXQX9MqiojBSO4mnnIb4"
#define USER_EMAIL "esp32@test.com"
#define USER_PASSWORD "cap413"

String idToken = "";
String refreshToken = "";
unsigned long tokenExpiryTime = 0;

RTC_DS3231 rtc;

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

    JsonDocument doc;
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
    relayMap["airPump"]["booleanValue"] = data.relayStates.airPump;
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
    }

    https.end();
}


void pushBatchLogToFirestore(RealTimeData *buffer, int size) {
    if (millis() > tokenExpiryTime) refreshIdToken();

    DateTime now = rtc.now();
    char dateStr[11];  // YYYY-MM-DD + null
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", now.year(), now.month(), now.day());

    String timestamp = String(millis());

    String url = "https://firestore.googleapis.com/v1/projects/" FIREBASE_PROJECT_ID "/databases/(default)/documents/sensor_logs/" + String(dateStr) + "_" + timestamp;

    // Adjust size if batch is large
    const size_t docSize = 8192 + (size * 512);
    DynamicJsonDocument doc(docSize);

    JsonObject fields = doc["fields"].to<JsonObject>();
    JsonObject logs = fields["logs"].to<JsonObject>();
    JsonObject arrayValue = logs["arrayValue"].to<JsonObject>();
    JsonArray values = arrayValue["values"].to<JsonArray>();

    for (int i = 0; i < size; i++) {
        JsonObject entry = values.add<JsonObject>();
        JsonObject mapVal = entry["mapValue"]["fields"].to<JsonObject>();

        mapVal["waterTemp"]["doubleValue"] = buffer[i].waterTemp;
        mapVal["pH"]["doubleValue"] = buffer[i].pH;
        mapVal["dissolvedOxygen"]["doubleValue"] = buffer[i].dissolvedOxygen;
        mapVal["turbidityNTU"]["doubleValue"] = buffer[i].turbidityNTU;
        mapVal["airTemp"]["doubleValue"] = buffer[i].airTemp;
        mapVal["airHumidity"]["doubleValue"] = buffer[i].airHumidity;
        mapVal["floatTriggered"]["booleanValue"] = buffer[i].floatTriggered;
    }

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
        Serial.println("[Firestore] Batch log pushed.");
    } else {
        Serial.print("[Firestore] Batch log push failed: ");
        Serial.println(https.errorToString(httpResponseCode));
    }

    https.end();
}