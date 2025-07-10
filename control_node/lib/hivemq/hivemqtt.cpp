#include "hivemq_mqtt.h"
#include <ArduinoMqttClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// === MQTT Config ===
const char* MQTT_BROKER = "5f44c70b14974776848ac3161f348729.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "aquabell_.hivemq";
const char* MQTT_PASS = "aquabellhiveMQ413";
const char* MQTT_TOPIC_JSON = "aquabell/control_node/data";

// === Individual topics ===
const char* MQTT_TOPICS[] = {
    "aquabell/sensors/water_temp",
    "aquabell/sensors/ph",
    "aquabell/sensors/do",
    "aquabell/sensors/turbidity",
    "aquabell/sensors/air_temp",
    "aquabell/sensors/humidity",
    "aquabell/sensors/float_switch"
};

WiFiClientSecure net;
MqttClient mqttClient(net);

// === Setup MQTT & SSL ===
void mqtt_init() {
    net.setInsecure();  // Disable cert verification (dev only)
    mqttClient.setId("control-node");
    mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);

    Serial.print("🔌 Connecting to MQTT...");
    if (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
        Serial.printf("❌ MQTT Connect failed: %s\n", mqttClient.connectError());
    } else {
        Serial.println("✅ MQTT connected");
    }
}

// === Keep connection alive ===
void mqtt_loop() {
    mqttClient.poll();

    if (!mqttClient.connected()) {
        Serial.println("🔁 MQTT disconnected. Reconnecting...");

        if (!mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
            Serial.printf("❌ MQTT reconnect failed: %s\n", mqttClient.connectError());
        } else {
            Serial.println("✅ MQTT reconnected");
        }
    }
}


// === Check if connected ===
bool mqtt_is_connected() {
    return mqttClient.connected();
}
// === Publish individual sensor values ===
void mqtt_publish_all(const RealTimeData& data) {
    mqtt_publish_float(MQTT_TOPICS[0], data.waterTemp);
    mqtt_publish_float(MQTT_TOPICS[1], data.pH);
    mqtt_publish_float(MQTT_TOPICS[2], data.dissolvedOxygen);
    mqtt_publish_float(MQTT_TOPICS[3], data.turbidityNTU);
    mqtt_publish_float(MQTT_TOPICS[4], data.airTemp);
    mqtt_publish_float(MQTT_TOPICS[5], data.airHumidity);
    mqtt_publish_bool(MQTT_TOPICS[6], data.floatTriggered);
}

void mqtt_publish_float(const char* topic, float value) {
    if (!mqttClient.connected()) return;
    mqttClient.beginMessage(topic);
    mqttClient.print(value, 2);
    mqttClient.endMessage();
    Serial.printf("📤 [%s] = %.2f\n", topic, value);
}

void mqtt_publish_bool(const char* topic, bool value) {
    if (!mqttClient.connected()) return;
    mqttClient.beginMessage(topic);
    mqttClient.print(value ? "true" : "false");
    mqttClient.endMessage();
    Serial.printf("📤 [%s] = %s\n", topic, value ? "true" : "false");
}

// === Publish full JSON snapshot ===
void mqtt_publish_data(const RealTimeData& data) {
    if (!mqttClient.connected()) return;

    StaticJsonDocument<256> doc;
    doc["waterTemp"] = data.waterTemp;
    doc["pH"] = data.pH;
    doc["DO"] = data.dissolvedOxygen;
    doc["turbidity"] = data.turbidityNTU;
    doc["airTemp"] = data.airTemp;
    doc["humidity"] = data.airHumidity;
    doc["float"] = data.floatTriggered;

    char buffer[256];
    serializeJson(doc, buffer);

    mqttClient.beginMessage(MQTT_TOPIC_JSON);
    mqttClient.print(buffer);
    mqttClient.endMessage();

    Serial.printf("📤 JSON ➜ [%s]: %s\n", MQTT_TOPIC_JSON, buffer);
}
