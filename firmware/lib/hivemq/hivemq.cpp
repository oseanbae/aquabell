#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.h"
//#include "mqtt_topics.h"
//#include "hivemq_root_ca.h"

// WiFiClientSecure secureClient;
// PubSubClient client(secureClient);


WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup_wifi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected.");
}

void setup_mqtt() {
  //  wifiClient.setCACert(HIVEMQ_ROOT_CA);  // << Inject CA cert
    client.setServer(MQTT_BROKER, MQTT_PORT); //8883
    client.setBufferSize(2048);

    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("ESP32_AquaBell")) {
            Serial.println(" connected.");
            //client.subscribe(actuator_topic);
        } else {
            Serial.print(" failed, rc=");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

void mqtt_loop() {
    if (!client.connected()) {
        setup_mqtt();
    }
    client.loop();
}

void mqtt_publish(const char* topic, const String& payload) {
    if (!client.connected()) setup_mqtt();
    client.publish(topic, payload.c_str());
}

void mqtt_set_callback(MQTT_CALLBACK_SIGNATURE) {
    client.setCallback(callback);
}