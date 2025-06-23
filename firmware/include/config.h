#ifndef CONFIG_H
#define CONFIG_H

// ============================
// === ADC Configuration ====
// ============================
#define ADC_MAX              4095.0    // 12-bit resolution
#define VOLTAGE_REF          3.3       // ADC reference voltage (V)

// ============================
// === DHT Sensor (Temp/RH) ===
// ============================
#define DHT_PIN              32
#define DHT_TYPE             DHT11
#define DHT_READ_INTERVAL    60000     // DHT sensor read interval (ms)

// ============================
// === DS18B20 Sensor (Water Temp) ===
// ============================
#define ONE_WIRE_BUS         35
#define DS18B20_READ_INTERVAL 60000    // DS18B20 read interval (ms)

// ============================
// === Turbidity Sensor =======
// ============================
#define TURBIDITY_PIN        36
#define NUM_SAMPLES          10
#define R1                   10000.0f
#define R2                   20000.0f
#define SENSOR_VOLTAGE_GAIN  1.5f
#define MAX_ADC_SAFE_VOLTAGE 3.1
#define TURBIDITY_READ_INTERVAL 10000  // Turbidity sensor read interval (ms)

// ============================
// === pH Sensor ==============
// ============================
#define PH_SENSOR_PIN        39
#define PH_CALIBRATION_OFFSET 0.0
#define PH_READ_INTERVAL     30000     // pH sensor read interval (ms)

// ============================
// === Dissolved Oxygen Sensor ===
// ============================
#define DO_SENSOR_PIN        34
#define DO_CAL_TEMP          30.75     // Calibration temp (°C)
#define DO_CAL_VOLTAGE       1650.0    // Voltage (mV) at calibration
#define DO_SENSOR_SAMPLES    32
#define DO_READ_INTERVAL     15000     // DO sensor read interval (ms)

// ============================
// === Float Switch ==========
// ============================
#define FLOAT_SWITCH_PIN     33
#define FLOAT_SWITCH_TRIGGERED LOW     // Set to HIGH if your float switch is active high

// ============================
// === RTC (DS3231) ===========
// ============================
#define RTC_SDA              21
#define RTC_SCL              22

// ============================
// === Relay Pins =============
// ============================
#define FAN_RELAY_PIN        19
#define LIGHT_RELAY_PIN      18
#define PUMP_RELAY_PIN       5
#define AIR_RELAY_PIN        17
#define VALVE_RELAY_PIN      16

// ============================
// === Rules / Thresholds =====
// ============================
#define TEMP_THRESHOLD      30.0f    // Trigger early to prevent stress
#define TEMP_EMERGENCY      32.0f    // Force fan ON regardless of RH
#define HUMIDITY_THRESHOLD  85.0f    // Use only *with* high temp
#define DISSOLVED_OXYGEN_THRESHOLD  5.0f   // DO threshold for valve logic
#define TURBIDITY_THRESHOLD         100.0f // Turbidity limit for pump logic

// ============================
// == Schedule Configuration ==
// ============================
#define PUMP_ON_DURATION       15U    // Pump ON duration (minutes)
#define PUMP_OFF_DURATION      45U    // Pump OFF duration (minutes)
#define PUMP_CYCLE_DURATION    (PUMP_ON_DURATION + PUMP_OFF_DURATION)  // Total cycle time

#define LIGHT_MORNING_ON     330      // 5:30 AM in minutes
#define LIGHT_MORNING_OFF    540      // 9:00 AM
#define LIGHT_EVENING_ON     900      // 3:00 PM
#define LIGHT_EVENING_OFF    1080     // 6:00 PM

// ============================
// === Fan Runtime Control ====
// ============================
#define FAN_MINUTE_RUNTIME   (5UL * 60 * 1000)  // 5 minutes in ms

// ============================
// === Logging ================
// ============================
// #define ENABLE_LOGGING     // Uncomment to enable Serial logging

#endif
