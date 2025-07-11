#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "sensor_data.h"

// Replace this with the actual MAC address of ESP32-B (control node)
extern uint8_t controlMAC[6];
void espnow_init();
bool sendESPNow(const RealTimeData& data);
