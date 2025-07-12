#pragma once
#include <Arduino.h>

extern const char* sensor_topic;
extern const char* actuator_topic;
void mqtt_message_callback(char* topic, byte* payload, unsigned int length);
