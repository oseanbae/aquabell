#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "sensor_data.h"

void mqtt_init();
void mqtt_loop();
void mqtt_publish_data(const RealTimeData& data);
void mqtt_publish_all(const RealTimeData& data);
bool mqtt_is_connected();