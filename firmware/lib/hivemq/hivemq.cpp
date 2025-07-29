//SENSOR TOPICS 
//#aquabell/sensor/waterTemp
//#aquabell/sensor/pH
//#aquabell/sensor/do
//#aquabell/sensor/turbidity
//#aquabell/sensor/airTemp
//#aquabell/sensor/airHumidity
//#aquabell/sensor/floatSwitch

//RELAY TOPICS
//#aquabell/relay/growLight
//#aquabell/relay/fan
//#aquabell/relay/waterPump
//#aquabell/relay/airPump
//#aquabell/relay/valve
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h" // WIFI + MQTT credentials
#include "cert.h"  // root CA cert

// Global client objects
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

void setupMQTT() {
    secureClient.setCACert(root_ca);  // Set CA certificate from cert.h
    mqttClient.setServer(MQTT_BROKER, 8883);  // HiveMQ TLS port
    mqttClient.setKeepAlive(60); // Optional: keep alive time
}

bool connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");

        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
            Serial.println("connected.");
            return true;
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5s...");
            delay(5000);
        }
    }
    return false;
}

bool isMQTTConnected() {
    return mqttClient.connected();
}

void publishMQTT(const char* topic, const char* payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic, payload);
    }
}

void loopMQTT() {
    mqttClient.loop();
}

void subscribeMQTT(const char* topic) {
    mqttClient.subscribe(topic);
}

void setMQTTCallback(MQTT_CALLBACK_SIGNATURE) {
    mqttClient.setCallback(callback);
}
