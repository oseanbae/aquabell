#ifndef CONFIG_H
#define CONFIG_H

// ============================
// === ADC / General =========
// ============================
#define ADC_MAX               4095.0
#define VOLTAGE_REF           3.3

// ============================
// === DHT Sensor (Air Temp/RH)
// ============================
#define DHT_PIN               32
#define DHT_TYPE              DHT11
#define DHT_READ_INTERVAL     60000  // ms

// Fan Control Thresholds (with hysteresis)
#define TEMP_ON_THRESHOLD     28.0f
#define TEMP_OFF_THRESHOLD    26.5f
#define HUMIDITY_ON_THRESHOLD 85.0f
#define HUMIDITY_OFF_THRESHOLD 75.0f
#define TEMP_EMERGENCY        32.0f

// ============================
// === DS18B20 (Water Temp) ===
// ============================
#define ONE_WIRE_BUS             35
#define DS18B20_READ_INTERVAL    60000  // ms

#define TEMP_CRITICAL_LOW(temp)   ((temp) < 18.0)
#define TEMP_CAUTION_LOW(temp)    ((temp) >= 18.0 && (temp) < 22.0)
#define TEMP_OPTIMAL(temp)        ((temp) >= 22.0 && (temp) <= 30.0)
#define TEMP_CAUTION_HIGH(temp)   ((temp) > 30.0 && (temp) <= 32.0)
#define TEMP_CRITICAL_HIGH(temp)  ((temp) > 32.0)

#define TEMP_STATUS_LABEL(temp) \
    (TEMP_CRITICAL_LOW(temp)  ? "Critical Low"  : \
    TEMP_CAUTION_LOW(temp)   ? "Caution Low"   : \
    TEMP_OPTIMAL(temp)       ? "Optimal"       : \
    TEMP_CAUTION_HIGH(temp)  ? "Caution High"  : \
    TEMP_CRITICAL_HIGH(temp) ? "Critical High" : "Unknown")

// ============================
// === Fan Runtime Control ====
// ============================
#define FAN_MINUTE_RUNTIME     (5UL * 60 * 1000)    // 5 min in ms
#define FAN_MAX_CONTINUOUS_MS  (60UL * 60 * 1000)   // 1 hour safety limit

// ============================
// === Pump Control =========
// ============================
#define PUMP_ON_DURATION       15U   // minutes
#define PUMP_OFF_DURATION      45U   // minutes
#define PUMP_CYCLE_DURATION    (PUMP_ON_DURATION + PUMP_OFF_DURATION)

// ============================
// === Grow Light Schedule ===
// ============================
#define LIGHT_MORNING_ON       330   // 5:30 AM
#define LIGHT_MORNING_OFF      540   // 9:00 AM
#define LIGHT_EVENING_ON       900   // 3:00 PM
#define LIGHT_EVENING_OFF      1080  // 6:00 PM

// ============================
// === Turbidity =============
// ============================
#define TURBIDITY_PIN           36
#define TURBIDITY_READ_INTERVAL 10000  // ms
#define NUM_SAMPLES             10
#define R1                      10000.0f
#define R2                      20000.0f
#define SENSOR_VOLTAGE_GAIN     1.5f
#define MAX_ADC_SAFE_VOLTAGE    3.1f
#define TURBIDITY_THRESHOLD     100.0f

#define NTU_OPTIMAL_CLEAR(ntu)      ((ntu) <= 50)
#define NTU_ACCEPTABLE(ntu)         ((ntu) > 50 && (ntu) <= 100)
#define NTU_MARGINAL(ntu)           ((ntu) > 100 && (ntu) <= 200)
#define NTU_POOR(ntu)               ((ntu) > 200 && (ntu) <= 500)
#define NTU_UNACCEPTABLE(ntu)       ((ntu) > 500)

#define NTU_STATUS_LABEL(ntu) \
    (NTU_OPTIMAL_CLEAR(ntu) ? "Clear" : \
    NTU_ACCEPTABLE(ntu) ? "Acceptable" : \
    NTU_MARGINAL(ntu) ? "Marginal" : \
    NTU_POOR(ntu) ? "Poor" : \
    NTU_UNACCEPTABLE(ntu) ? "Very Poor" : "Unknown")

// ============================
// === pH Sensor =============
// ============================
#define PH_SENSOR_PIN         39
#define PH_CALIBRATION_OFFSET 0.0
#define PH_READ_INTERVAL      30000  // ms

#define PH_CRITICAL_LOW(pH)   ((pH) < 6.0)
#define PH_CAUTION_LOW(pH)    ((pH) >= 6.0 && (pH) < 6.5)
#define PH_OPTIMAL(pH)        ((pH) >= 6.5 && (pH) <= 7.5)
#define PH_CAUTION_HIGH(pH)   ((pH) > 7.5 && (pH) <= 8.0)
#define PH_CRITICAL_HIGH(pH)  ((pH) > 8.0)

#define PH_STATUS_LABEL(pH) \
    (PH_CRITICAL_LOW(pH) ? "Critical Low" : \
    PH_CAUTION_LOW(pH) ? "Caution Low" : \
    PH_OPTIMAL(pH) ? "Optimal" : \
    PH_CAUTION_HIGH(pH) ? "Caution High" : \
    PH_CRITICAL_HIGH(pH) ? "Critical High" : "Unknown")

// ============================
// === DO Sensor =============
// ============================
#define DO_SENSOR_PIN         34
#define DO_CAL_TEMP           30.75f
#define DO_CAL_VOLTAGE        1650.0f
#define DO_SENSOR_SAMPLES     32
#define DO_READ_INTERVAL      15000  // ms

#define DO_CRITICAL_LOW(do)   ((do) < 4.0)
#define DO_CAUTION_LOW(do)    ((do) >= 4.0 && (do) < 5.0)
#define DO_ACCEPTABLE(do)     ((do) >= 5.0 && (do) < 6.0)
#define DO_OPTIMAL(do)        ((do) >= 6.0 && (do) <= 8.0)
#define DO_UPPER_SAFE(do)     ((do) > 8.0)

#define DO_STATUS_LABEL(do) \
    (DO_CRITICAL_LOW(do) ? "Critical Low" : \
    DO_CAUTION_LOW(do) ? "Caution Low" : \
    DO_ACCEPTABLE(do) ? "Acceptable" : \
    DO_OPTIMAL(do) ? "Optimal" : \
    DO_UPPER_SAFE(do) ? "Upper Safe" : "Unknown")

// ============================
// === Float Switch ==========
// ============================
#define FLOAT_SWITCH_PIN      33
#define FLOAT_SWITCH_TRIGGERED LOW

// ============================
// === RTC Pins ==============
// ============================
#define RTC_SDA               21
#define RTC_SCL               22
#define RTC_ADDRESS           0x68

// ============================
// === 16x2 I2C LCD Display ===
// ============================
#define LCD_SDA               21
#define LCD_SCL               22
#define LCD_ADDR              0x27
#define LCD_COLS              16
#define LCD_ROWS              2
#define LCD_IDLE_TIMEOUT      15000  // ms
#define LCD_BACKLIGHT_ON      true
#define LCD_BACKLIGHT_OFF     false
#define DEBOUNCE_DELAY        50    // ms
#define BUTTON_NEXT           25
#define BUZZER_PIN            26
#define LED_PIN               27
#define TOTAL_PAGES           3


// ============================
// === Relay Control ==========
// ============================
#define FAN_RELAY_PIN         19
#define LIGHT_RELAY_PIN       18
#define PUMP_RELAY_PIN        5
#define AIR_RELAY_PIN         17
#define VALVE_RELAY_PIN       16

// ============================
// === Optional Logging =======
// ============================
// #define ENABLE_LOGGING     // Uncomment to enable Serial output

#endif
