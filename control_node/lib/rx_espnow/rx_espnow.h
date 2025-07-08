#pragma once

void espnow_rx_init();
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incoming, int len);