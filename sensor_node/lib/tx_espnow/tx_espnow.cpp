#include "tx_espnow.h"

uint8_t controlMAC[6] = { 0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF }; // <-- Update to match receiver

static bool peerAdded = false;
static bool sendSuccess = false;

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    sendSuccess = (status == ESP_NOW_SEND_SUCCESS);

    Serial.print("ESP-NOW Send to ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", mac_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.println(sendSuccess ? " succeeded." : " failed.");
}

void espnow_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Disconnect to avoid WiFi interference

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed");
        return;
    }

    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, controlMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(controlMAC)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("❌ Failed to add ESP-NOW peer.");
        } else {
            peerAdded = true;
            Serial.println("✅ ESP-NOW peer added.");
        }
    } else {
        peerAdded = true;
        Serial.println("ℹ️ ESP-NOW peer already exists.");
    }
}

bool sendESPNow(const RealTimeData& data) {
    if (!peerAdded) {
        Serial.println("⚠️ Cannot send: ESP-NOW peer not initialized.");
        return false;
    }

    sendSuccess = false; // Reset before send
    esp_err_t result = esp_now_send(controlMAC, (uint8_t*)&data, sizeof(data));

    if (result != ESP_OK) {
        Serial.printf("❌ ESP-NOW send failed: error %d\n", result);
        return false;
    }

    // Note: sendSuccess will be set asynchronously in onDataSent()
    // Optionally: wait short time here if you need confirmation (not ideal in real-time loops)

    return true; // Assume success for now — real result is in callback
}
