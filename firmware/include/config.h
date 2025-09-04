#pragma once

// === ADC / General =========
#define ADC_MAX               4095.0
#define VOLTAGE_REF           3.3f
#define ADC_VOLTAGE_MV        3300.0f // Reference voltage in mV

// === DHT Sensor (Air Temp/RH)
#define DHT_PIN               23
#define DHT_TYPE              11

// === DS18B20 (Water Temp) ===
#define ONE_WIRE_BUS             25

// === Turbidity =============
#define TURBIDITY_PIN           35
#define NUM_SAMPLES             10
#define R1                      10000.0f
#define R2                      20000.0f
#define SENSOR_VOLTAGE_GAIN     ((R1 + R2) / R2)  // = 1.5
// NTU Mapping Constants (linear)
#define NTU_SLOPE                -375.0f
#define NTU_OFFSET               1125.0f


// === pH Sensor =============
#define PH_SENSOR_PIN         36
#define PH_SENSOR_SAMPLES     30
#define PH_SAMPLE_DELAY_MS    8  // Delay between samples (ms)

// Calibration points (Two-Point Method)
#define CAL_PH1               7.0       // First calibration pH value (neutral)
#define CAL_VOLTAGE1_MV       1620.0    // Voltage at pH 7 (mV)
#define CAL_PH2               4.0       // Second calibration pH value (acidic)
#define CAL_VOLTAGE2_MV       1555.0    // Voltage at pH 4 (mV)

// === DO Sensor =============
#define DO_SENSOR_PIN         34
#define DO_CAL_TEMP           31.37    // °C from your calibration
#define DO_CAL_VOLTAGE        453.14   // mV from your calibration
#define DO_SENSOR_SAMPLES     32

// === Float Switch ==========
#define FLOAT_SWITCH_PIN      32
#define FLOAT_SWITCH_TRIGGERED LOW

// === SENSOR READ INTERVALS ===
#define DHT_READ_INTERVAL     60000  // ms
#define DS18B20_READ_INTERVAL 60000  // ms
#define TURBIDITY_READ_INTERVAL 10000  // ms
#define PH_READ_INTERVAL      30000  // ms
#define DO_READ_INTERVAL      15000  // ms
#define FLOAT_READ_INTERVAL       5000  // Avoid high-frequency polling

// === SENSOR AVAILABILITY (for testing) ===
#define SKIP_PH_SENSOR        true   // Set to true if pH sensor not connected
#define SKIP_DO_SENSOR        true  // Set to true if DO sensor not connected
#define SKIP_DHT_SENSOR       true  // Set to true if DHT sensor not connected

// === Relay Control ==========
#define FAN_RELAY_PIN         19
#define LIGHT_RELAY_PIN       18
#define PUMP_RELAY_PIN        5
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

#define MINUTES(h, m) ((h) * 60 + (m))

#define LIGHT_MORNING_ON   MINUTES(5, 30)   // 5:30 AM
#define LIGHT_MORNING_OFF  MINUTES(9, 0)    // 9:00 AM
#define LIGHT_EVENING_ON   MINUTES(15, 0)   // 3:00 PM
#define LIGHT_EVENING_OFF  MINUTES(18, 0)   // 6:00 PM


//=== WIFI Configuration ===
//SHIELA'S WIFI
// #define WIFI_SSID          "SHIBOL"
// #define WIFI_PASS      "SPES2025_"

//JP'S WIFI
#define WIFI_SSID   "meow"
#define WIFI_PASS   "helloworld2025"

//Capstone wifi
// #define WIFI_SSID           "Capstone"
// #define WIFI_PASS           "capstone"