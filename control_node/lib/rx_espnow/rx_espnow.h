#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "sensor_data.h"

extern volatile RealTimeData latestData;
extern volatile bool dataAvailable;
extern unsigned long lastDataReceived;

void espnow_rx_init(int channel); // Default to channel 1
