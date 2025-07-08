#include <Arduino.h>
#include <Wire.h>
#include <LITTLEFS.h>
#include "esp_task_wdt.h"
#include "relay_control.h"
#include "sensor_data.h"
#include "rx_espnow.h"
#include "rule_engine.h"
#include "RTClib.h"

#define WDT_TIMEOUT_SECONDS 5

RTC_DS3231 rtc;

extern volatile RealTimeData latestData;
extern volatile bool dataAvailable;
extern unsigned long lastDataReceived;

void log_batch_to_file(const RealTimeData& data, DateTime timestamp) {
    File file = LittleFS.open("/batch_log.txt", FILE_APPEND);
    if (!file) {
        Serial.println("❌ Failed to open log file");
        return;
    }

    file.printf("{\"time\":\"%04d-%02d-%02dT%02d:%02d:%02d\",\"waterTemp\":%.2f,\"pH\":%.2f,\"DO\":%.2f,\"turbidity\":%.2f,\"airTemp\":%.2f,\"humidity\":%.2f,\"float\":%d}\n",
        timestamp.year(), timestamp.month(), timestamp.day(),
        timestamp.hour(), timestamp.minute(), timestamp.second(),
        data.waterTemp, data.pH, data.dissolvedOxygen,
        data.turbidityNTU, data.airTemp, data.airHumidity,
        data.floatTriggered);

    file.close();
    Serial.println("📝 Batch snapshot saved.");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!rtc.begin()) {
        Serial.println("❌ RTC not found");
        while (1);
    }

    if (!LittleFS.begin()) {
        Serial.println("❌ Failed to mount LittleFS");
        while (1);
    }

    relay_control_init();
    espnow_rx_init();

    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);

    Serial.println("✅ LittleFS mounted");
    Serial.println("🚀 Control Node Ready");
}

void loop() {
    esp_task_wdt_reset();

    if (dataAvailable) {
        RealTimeData copy;
        memcpy(&copy, (const void*)&latestData, sizeof(RealTimeData));
        dataAvailable = false;

        apply_rules(copy);  // ✅ Your automation engine

        if (copy.isBatch) {
            DateTime now = rtc.now();
            log_batch_to_file(copy, now);  // ✅ Write snapshot
        }
    } else if (millis() - lastDataReceived > 10000) {
        Serial.println("⚠️ No sensor data received in 10 seconds.");
    }

    delay(1000);
}
