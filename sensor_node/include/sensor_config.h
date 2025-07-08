#ifndef CONFIG_H
#define CONFIG_H


// === ADC / General =========
#define ADC_MAX               4095.0
#define VOLTAGE_REF           3.3f
#define ADC_VOLTAGE_MV        3300.0f // Reference voltage in mV

// === DHT Sensor (Air Temp/RH)
#define DHT_PIN               32
#define DHT_TYPE              11
#define DHT_READ_INTERVAL     60000  // ms

// === DS18B20 (Water Temp) ===
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

// === Turbidity =============
#define TURBIDITY_PIN           36
#define TURBIDITY_READ_INTERVAL 10000  // ms
#define NUM_SAMPLES             10
#define R1                      10000.0f
#define R2                      20000.0f
#define SENSOR_VOLTAGE_GAIN     ((R1 + R2) / R2)  // = 1.5

// NTU Mapping Constants (linear)
#define NTU_SLOPE                -375.0f
#define NTU_OFFSET               1125.0f

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

// === pH Sensor =============
#define PH_SENSOR_PIN         39
#define PH_SENSOR_SAMPLES     100
#define PH_SAMPLE_DELAY_MS    20 // ms delay between samples
#define NEUTRAL_PH            7.0       // neutral pH
#define NEUTRAL_VOLTAGE_MV    1546.50     // << Measure actual voltage when in pH 7 buffer
#define MV_PER_PH            -59.16    // Approximate Nernst slope at 25°C
#define PH_READ_INTERVAL      30000    // ms 

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

// === DO Sensor =============
#define DO_SENSOR_PIN         34
#define DO_CAL_TEMP           31.37    // °C from your calibration
#define DO_CAL_VOLTAGE        453.14   // mV from your calibration
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

// === Float Switch ==========
#define FLOAT_SWITCH_PIN      33
#define FLOAT_SWITCH_TRIGGERED LOW

// === RTC Pins ==============
#define RTC_SDA               21
#define RTC_SCL               22
#define RTC_ADDRESS           0x68

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

 #define BATCH_SEND_INTERVAL 300000UL // 5 minutes in milliseconds
// === Optional Logging =======
// #define ENABLE_LOGGING     // Uncomment to enable Serial output

#endif
