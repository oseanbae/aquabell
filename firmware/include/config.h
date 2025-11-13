#pragma once

// === ADC / General =========
#define ADC_MAX               4095.0
#define VOLTAGE_REF           3.3f
#define ADC_VOLTAGE_MV        3300.0f // Reference voltage in mV

// === DHT Sensor (Air Temp/RH)
#define DHT_PIN               23
#define DHT_TYPE              11
// Fan Control Thresholds (with hysteresis)
#define TEMP_ON_THRESHOLD      29.0f
#define TEMP_OFF_THRESHOLD     26.5f
#define HUMIDITY_MAX_THRESHOLD 75.0f
#define HUMIDITY_MIN_THRESHOLD 62.0f

// === DS18B20 (Water Temp) ===
#define ONEWIRE_BUS      25  
#define COOLER_ON_TEMP   29.0f
#define COOLER_OFF_TEMP  27.5f
#define HEATER_ON_TEMP   22.0f
#define HEATER_OFF_TEMP  23.5f

// === pH Control ===
#define PH_LOW_THRESHOLD      6.45f   // trigger UP pump
#define PH_LOW_OFF            6.65f   // turn UP pump off
#define PH_HIGH_THRESHOLD     7.55f   // trigger DOWN pump
#define PH_HIGH_OFF           7.35f   // turn DOWN pump off
#define PH_PUMP_RUN_MS       2000UL  // Duration of pH dose (2 seconds)
#define PH_MIN_CHECK_INTERVAL_MS 60000UL  // Minimum interval between pH checks (1 minute)
#define PH_REST_PERIOD_MS    300000UL // Rest period after dose before next check (5 minutes)

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
#define DO_CAL_VOLTAGE        453.14   // mV from your calibration 1135.0
#define DO_SENSOR_SAMPLES     32

// === Float Switch ==========
#define FLOAT_SWITCH_PIN      32
#define FLOAT_SWITCH_TRIGGERED LOW

// === SENSOR READ INTERVALS ===
#define UNIFIED_SENSOR_INTERVAL 10000

// === Relay Control ==========
#define FAN_RELAY_PIN          19
#define LIGHT_RELAY_PIN        18
#define PUMP_RELAY_PIN         5
#define VALVE_RELAY_PIN        17
#define WATER_COOLER_RELAY_PIN 16
#define WATER_HEATER_RELAY_PIN 4
#define PH_LOWERING_RELAY_PIN  2
#define PH_RAISING_RELAY_PIN   15

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
#define FLOAT_HIGH_DEBOUNCE_MS  3000UL   // require high for 3s


#define RELAY_SYNC_COOLDOWN 2000UL

//=== WIFI Configuration ===
//JP'S HOME WIFI
#define WIFI_SSID   "meow"
#define WIFI_PASS   "helloworld2025"

//Capstone wifi
// #define WIFI_SSID           "Capstone"
// #define WIFI_PASS           "capstone"
// #define WIFI_SSID           "Capstone-2.4G-ext"

//IPEN WIFI
// #define WIFI_SSID           "Ipen"
// #define WIFI_PASS           "precious1965"

//JP's PHONE HOTSPOT
// #define WIFI_SSID           "jp"
// #define WIFI_PASS           "11223344"


//GOMO WIFI
// #define WIFI_SSID           "OREOCHROMIS"
// #define WIFI_PASS           "AquaCapsicum"

// #define WIFI_SSID           "OPPO A54"
// #define WIFI_PASS           "12345678"

// #define WIFI_SSID           "PSU_HR"
// #define WIFI_PASS           "PSUHR2K25"