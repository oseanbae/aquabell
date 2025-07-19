#pragma once

// === ADC / General =========
#define ADC_MAX               4095.0
#define VOLTAGE_REF           3.3f
#define ADC_VOLTAGE_MV        3300.0f // Reference voltage in mV

// === DHT Sensor (Air Temp/RH)
#define DHT_PIN               23
#define DHT_TYPE              11

// === DS18B20 (Water Temp) ===
#define ONE_WIRE_BUS             35

// === Turbidity =============
#define TURBIDITY_PIN           36
#define NUM_SAMPLES             10
#define R1                      10000.0f
#define R2                      20000.0f
#define SENSOR_VOLTAGE_GAIN     ((R1 + R2) / R2)  // = 1.5
// NTU Mapping Constants (linear)
#define NTU_SLOPE                -375.0f
#define NTU_OFFSET               1125.0f


// === pH Sensor =============
#define PH_SENSOR_PIN         39
#define PH_SENSOR_SAMPLES     100
#define PH_SAMPLE_DELAY_MS    20 // ms delay between samples
#define NEUTRAL_PH            7.0       // neutral pH
#define NEUTRAL_VOLTAGE_MV    1546.50     // << Measure actual voltage when in pH 7 buffer
#define MV_PER_PH            -59.16    // Approximate Nernst slope at 25°C

// === DO Sensor =============
#define DO_SENSOR_PIN         34
#define DO_CAL_TEMP           31.37    // °C from your calibration
#define DO_CAL_VOLTAGE        453.14   // mV from your calibration
#define DO_SENSOR_SAMPLES     32

// === Float Switch ==========
#define FLOAT_SWITCH_PIN      33
#define FLOAT_SWITCH_TRIGGERED LOW

// === 16x2 I2C LCD Display ===
#define LCD_SDA               21
#define LCD_SCL               22
#define LCD_ADDR              0x27
#define LCD_COLS              16
#define LCD_ROWS              2
#define LCD_IDLE_TIMEOUT      15000  // ms
#define LCD_BACKLIGHT_ON      true
#define LCD_BACKLIGHT_OFF     false
#define DEBOUNCE_DELAY        50    // ms
#define BUTTON_NEXT           16 
#define BUZZER_PIN            17
#define LED_PIN               18
#define TOTAL_PAGES           3

// === SENSOR READ INTERVALS ===
#define DHT_READ_INTERVAL     60000  // ms
#define DS18B20_READ_INTERVAL 60000  // ms
#define TURBIDITY_READ_INTERVAL 10000  // ms
#define PH_READ_INTERVAL      30000  // ms
#define DO_READ_INTERVAL      15000  // ms
#define BATCH_SEND_INTERVAL 300000UL // 5 minutes in milliseconds

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

#define MINUTES(h, m) ((h) * 60 + (m))

#define LIGHT_MORNING_ON   MINUTES(5, 30)   // 5:30 AM
#define LIGHT_MORNING_OFF  MINUTES(9, 0)    // 9:00 AM
#define LIGHT_EVENING_ON   MINUTES(15, 0)   // 3:00 PM
#define LIGHT_EVENING_OFF  MINUTES(18, 0)   // 6:00 PM


// === RTC Pins ==============
#define RTC_SDA               21
#define RTC_SCL               22
#define RTC_ADDRESS           0x68

//=== WIFI Configuration ===
//JAY'S WIFI
// #define WIFI_SSID             "Tung Tung Tung Sahur"
// #define WIFI_PASSWORD         "SkibidiToilet_00"

//SHIELA'S WIFI
// #define WIFI_SSID          "Library_GuesntNetwork"
// #define WIFI_PASSWORD      "Sipuegg#19_"

//JP'S WIFI
#define WIFI_SSID   "meow"
#define WIFI_PASSWORD "helloworld2025"