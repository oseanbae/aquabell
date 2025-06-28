#include "tx_espnow.h"

uint8_t controlMAC[6] = { 0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF }; // <-- UPDATE THIS

void espnow_init() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, controlMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(controlMAC)) {
        esp_now_add_peer(&peerInfo);
    }
}

bool sendESPNow(const RealTimeData& data) {
    esp_err_t result = esp_now_send(controlMAC, (uint8_t*)&data, sizeof(data));
    return result == ESP_OK;
}
