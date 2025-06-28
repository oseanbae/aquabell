#pragma once

// === Relay Control ==========
#define FAN_RELAY_PIN         19
#define LIGHT_RELAY_PIN       18
#define PUMP_RELAY_PIN        5
#define AIR_RELAY_PIN         17
#define VALVE_RELAY_PIN       16

// === Fan Runtime Control ====
#define FAN_MINUTE_RUNTIME     (5UL * 60 * 1000)    // 5 min in ms
#define FAN_MAX_CONTINUOUS_MS  (60UL * 60 * 1000)   // 1 hour safety limit

// Fan Control Thresholds (with hysteresis)
#define TEMP_ON_THRESHOLD     28.0f
#define TEMP_OFF_THRESHOLD    26.5f
#define HUMIDITY_ON_THRESHOLD 85.0f
#define HUMIDITY_OFF_THRESHOLD 75.0f
#define TEMP_EMERGENCY        32.0f

// === Pump Control =========
#define PUMP_ON_DURATION       15U   // minutes
#define PUMP_OFF_DURATION      45U   // minutes
#define PUMP_CYCLE_DURATION    (PUMP_ON_DURATION + PUMP_OFF_DURATION)

// === Grow Light Schedule ===
#define LIGHT_MORNING_ON       330   // 5:30 AM
#define LIGHT_MORNING_OFF      540   // 9:00 AM
#define LIGHT_EVENING_ON       900   // 3:00 PM
#define LIGHT_EVENING_OFF      1080  // 6:00 PM

// === RTC Pins ==============
#define RTC_SDA               21
#define RTC_SCL               22
#define RTC_ADDRESS           0x68

//MQTT Broker Configuration
#define MQTT_BROKER           "5f44c70b14974776848ac3161f348729.s1.eu.hivemq.cloud  "
#define MQTT_PORT             8883
#define MQTT_USER             "aquabell_client"
#define MQTT_PASSWORD         "aquabellhiveMQ413"
#define SSL_CERTIFICATE "-----BEGIN CERTIFICATE-----\n...paste the full cert...\n-----END CERTIFICATE-----\n"


// WIFI Configuration
#define WIFI_SSID             "meow_5G"
#define WIFI_PASSWORD         "byeworld5g"
