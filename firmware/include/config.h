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
#define PH_SENSOR_PIN 39 // Pin for the pH sensor
#define PH_CALIBRATION_OFFSET 0.0  // Calibration offset for pH sensor

// === DO Sensor ===
#define DO_SENSOR_PIN 34 // Pin for the DO sensor
#define DO_CALIBRATION_OFFSET 0.0 // Calibration offset for DO sensor

// === DS3231 RTC Module ===
#define RTC_SDA 21 // I2C pins for ESP32
#define RTC_SCL 22 // I2C pins for ESP32

// === Relay Module ===
#define FAN_RELAY_PIN        19 // Pin for the fan relay
#define LIGHT_RELAY_PIN      18 // Pin for the light relay
#define PUMP_RELAY_PIN       5  // Pin for the pump relay
#define AIR_RELAY_PIN        17 // Pin for the air pump relay
#define VALVE_RELAY_PIN      16 // Pin for the valve relay

// === Rules Engine ===
#define TEMP_THRESHOLD 32.0f // Temperature threshold for fan control
#define HUMIDITY_THRESHOLD 90.0f // Humidity threshold for light control

// === Timing Configuration ===
#define PUMP_ON_DURATION 15      // Pump ON duration in minutes
#define PUMP_OFF_DURATION 45     // Pump OFF duration in minutes
#define PUMP_CYCLE_DURATION (PUMP_ON_DURATION + PUMP_OFF_DURATION) // Total cycle duration in minutes

#define LIGHT_MORNING_ON 360    // 6 * 60 = 6:00 AM
#define LIGHT_MORNING_OFF 600   // 10 * 60 = 10:00 AM
#define LIGHT_EVENING_ON 930    // 15 * 60 + 30 = 15:30 (3:30 PM)
#define LIGHT_EVENING_OFF 1170  // 19 * 60 + 30 = 19:30 (7:30 PM)

// === Logging ===
// Uncomment to enable Serial logging
 //#define ENABLE_LOGGING

#endif
