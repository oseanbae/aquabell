#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "sensor_data.h"  // Same packed struct as sender

extern "C" {
#include "esp_wifi.h"    // For esp_wifi_set_ps
}
RealTimeData incomingData;
volatile RealTimeData latestData;
volatile bool dataAvailable = false;
unsigned long lastDataReceived = 0;

#include "rx_espnow.h"
#include "sensor_data.h"

volatile RealTimeData latestData;
volatile bool dataAvailable = false;
unsigned long lastDataReceived = 0;

void onDataRecv(const uint8_t *mac_addr, const uint8_t *incoming, int len) {
    if (len != sizeof(RealTimeData)) {
        Serial.printf("⚠️ Invalid data size: %d bytes (expected %d)\n", len, sizeof(RealTimeData));
        return;
    }

    // ✅ Copy to shared struct
    memcpy((void*)&latestData, incoming, sizeof(RealTimeData));
    dataAvailable = true;
    lastDataReceived = millis();  // ✅ Keep track of time received

    // 📥 Debug print
    Serial.print("📥 Received from: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

    Serial.println(latestData.isBatch ? "📦 [Batch: 5-min average]" : "⚡ [Real-Time Update]");
    Serial.printf("Water Temp: %.2f°C\n", latestData.waterTemp);
    Serial.printf("pH: %.2f\n", latestData.pH);
    Serial.printf("DO: %.2f mg/L\n", latestData.dissolvedOxygen);
    Serial.printf("Turbidity: %.2f NTU\n", latestData.turbidityNTU);
    Serial.printf("Air Temp: %.2f°C\n", latestData.airTemp);
    Serial.printf("Humidity: %.2f%%\n", latestData.airHumidity);
    Serial.printf("Float Switch: %s\n", latestData.floatTriggered ? "TRIGGERED" : "OK");
    Serial.println("-------------------------------------------------");
}


void espnow_rx_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onDataRecv);
    Serial.println("✅ ESP-NOW receiver initialized.");
}
