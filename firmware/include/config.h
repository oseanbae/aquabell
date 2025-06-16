#ifndef CONFIG_H
#define CONFIG_H

// === Sensor Configuration for ESP32 ===
#define ADC_MAX 4095.0         // 12-bit resolution
#define VOLTAGE_REF 3.3        // ADC reference voltage (V)

// === DHT Sensor ===
#define DHT_PIN 32
#define DHT_TYPE DHT11

// === DS18B20 ===
#define ONE_WIRE_BUS 35

// === Turbidity Sensor ===
#define TURBIDITY_PIN 36
#define NUM_SAMPLES 10
#define R1 10000.0f
#define R2 20000.0f
#define SENSOR_VOLTAGE_GAIN 1.5f
#define MAX_ADC_SAFE_VOLTAGE 3.1

// === Float Switch ===
#define FLOAT_SWITCH_PIN 33
#define FLOAT_SWITCH_TRIGGERED LOW // Change to HIGH if your float switch is active high

// === pH Sensor ===
#define PH_SENSOR_PIN 39
#define PH_CALIBRATION_OFFSET 0.0

// === DO Sensor ===
#define DO_SENSOR_PIN 34
#define DO_CALIBRATION_OFFSET 0.0

// === DS3231 RTC Module ===
#define RTC_SDA 21
#define RTC_SCL 22

// === Relay Module ===
#define FAN_RELAY_PIN        19
#define LIGHT_RELAY_PIN      18
#define PUMP_RELAY_PIN       5
#define AIR_RELAY_PIN        17
#define VALVE_RELAY_PIN      16
// === Logging ===
// Uncomment to enable Serial logging
 //#define ENABLE_LOGGING

#endif
