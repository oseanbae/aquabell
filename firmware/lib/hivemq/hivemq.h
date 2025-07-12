#pragma once

#include <PubSubClient.h>

extern PubSubClient client;

void setup_wifi();
void setup_mqtt();
void mqtt_loop();
void mqtt_publish(const char* topic, const String& payload);
void mqtt_set_callback(MQTT_CALLBACK_SIGNATURE);
