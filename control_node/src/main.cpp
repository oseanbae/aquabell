#include <Arduino.h>
#include <Wire.h>

#include "esp_task_wdt.h"
#include "relay_control.h"
#include "sensor_data.h"
#include "rx_espnow.h"
#include "rule_engine.h"  // contains apply_rules()
#include "RTClib.h"

#define WDT_TIMEOUT_SECONDS 5

extern volatile RealTimeData latestData;
extern volatile bool dataAvailable;
extern unsigned long lastDataReceived;

RTC_DS3231 rtc;

void setup() {
    Serial.begin(115200);
    delay(1000);  // Allow serial to settle

    // Init RTC
    if (!rtc.begin()) {
        Serial.println("❌ RTC not found");
        while (1);  // Stop everything
    }

    // Optional: Set RTC time (do once, comment after)
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Init relay pins
    relay_control_init();

    // Init ESP-NOW
    espnow_rx_init();

    // Watchdog setup
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);  // Enable panic/reboot
    esp_task_wdt_add(NULL);  // Add main loop to watchdog

    Serial.println("🚀 Control Node Ready");
}

void loop() {
    esp_task_wdt_reset();  // Reset watchdog every loop

    if (dataAvailable) {
        RealTimeData copy;
        memcpy(&copy, (const void*)&latestData, sizeof(RealTimeData));
        apply_rules(copy);
        dataAvailable = false;
    } else {
        if (millis() - lastDataReceived > 10000) {
            Serial.println("⚠️ No sensor data received in 10 seconds.");
        }
    }
    delay(1000);  // Rule engine runs every second
}
