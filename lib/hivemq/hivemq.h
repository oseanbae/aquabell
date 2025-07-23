#pragma once

#include <PubSubClient.h>

void setupMQTT();
void connectMQTT();  // <-- match this to cpp
void publishMQTT(const char* topic, const char* payload);
void loopMQTT();
bool isMQTTConnected();
