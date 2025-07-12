#include "mqtt_topics.h"
#include <Arduino.h>

const char* sensor_topic = "aquabell/sensors";
const char* actuator_topic = "aquabell/actuators";

void mqtt_message_callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("[MQTT] Received message, but no handler defined.");
}