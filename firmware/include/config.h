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
#define UNIFIED_SENSOR_INTERVAL 10000

// === Relay Control ==========
#define FAN_RELAY_PIN         19
#define LIGHT_RELAY_PIN       18
#define PUMP_RELAY_PIN        5
#define VALVE_RELAY_PIN       16

// === Fan Runtime Control ====
#define FAN_MINUTE_RUNTIME     (5UL * 60 * 1000)    // 5 min in ms
#define FAN_MAX_CONTINUOUS_MS  (6UL * 60 * 60 * 1000)   // 1 hour safety limit

// Fan Control Thresholds (with hysteresis)
#define TEMP_ON_THRESHOLD     28.0f
#define TEMP_OFF_THRESHOLD    26.5f
#define HUMIDITY_ON_THRESHOLD 90.0f
#define TEMP_EMERGENCY        32.0f

// === Pump Control =========
#define PUMP_ON_DURATION       15U   // minutes
#define PUMP_OFF_DURATION      45U   // minutes


// === Lighting Control ======
// Define lighting schedule in minutes from midnight
#define MINUTES(h, m) ((h) * 60 + (m))

#define LIGHT_MORNING_ON   MINUTES(5, 30)   // 5:30 AM
#define LIGHT_MORNING_OFF  MINUTES(11, 0)   // 11:00 AM (Start of Siesta/Midday heat)
#define LIGHT_EVENING_ON   MINUTES(15, 0)   // 3:00 PM (End of Siesta/Midday heat)
#define LIGHT_EVENING_OFF  MINUTES(21, 30)  // 9:30 PM (End of total photoperiod)


// === Float/Valve Safety =====
// Debounce windows for float switch transitions
#define FLOAT_LOW_DEBOUNCE_MS   3000UL   // require low for 3s
#define FLOAT_HIGH_DEBOUNCE_MS  5000UL   // require high for 5s

// Maximum time to keep refill valve open before safety lock
#define VALVE_MAX_OPEN_MS       (30UL * 60UL * 1000UL)  // 30 minutes

//=== WIFI Configuration ===
//JP'S HOME WIFI
#define WIFI_SSID   "meow"
#define WIFI_PASS   "helloworld2025"

//Capstone wifi
// #define WIFI_SSID           "Capstone"
// #define WIFI_PASS           "capstone"

//IPEN WIFI
// #define WIFI_SSID           "Ipen"
// #define WIFI_PASS           "precious1965"

//JP's PHONE HOTSPOT
// #define WIFI_SSID           "jp"
// #define WIFI_PASS           "11223344"

//GOMO WIFI
// #define WIFI_SSID           "OREOCHROMIS"
// #define WIFI_PASS           "AquaCapsicum"