#pragma once

// ADC Configuration
#define ADC_MAX               4095.0
#define VOLTAGE_REF           3.3f
#define ADC_VOLTAGE_MV        3300.0f

// DHT (Air Temp/RH)
#define DHT_PIN               23
#define DHT_TYPE              11

// DS18B20 (Water Temp)
#define ONEWIRE_BUS           25

// Turbidity Sensor
#define TURBIDITY_PIN         35
#define NUM_SAMPLES           10
#define R1                    10000.0f
#define R2                    20000.0f
#define SENSOR_VOLTAGE_GAIN   ((R1 + R2) / R2)   // 1.5 gain
#define NTU_SLOPE             -375.0f
#define NTU_OFFSET            1125.0f

// pH Sensor
#define PH_SENSOR_PIN         39
#define PH_SENSOR_SAMPLES     30
#define PH_SAMPLE_DELAY_MS    8

// Two-point calibration
#define CAL_PH1               7.0
#define CAL_VOLTAGE1_MV       1620.0
#define CAL_PH2               4.0
#define CAL_VOLTAGE2_MV       1555.0

// DO Sensor
#define DO_SENSOR_PIN         34
#define DO_CAL_TEMP           31.37
#define DO_CAL_VOLTAGE        453.14
#define DO_SENSOR_SAMPLES     32

// Float Switch (Water Level)
#define FLOAT_SWITCH_PIN          32
#define FLOAT_LOW_DEBOUNCE_MS     3000UL // Time the float must remain low to confirm "low" state
#define FLOAT_HIGH_DEBOUNCE_MS    3000UL // Time the float must remain high to confirm "normal" state

// Unified Sensor Read Interval
#define UNIFIED_SENSOR_INTERVAL   10000UL

// Relay Pins
#define FAN_RELAY_PIN             19
#define LIGHT_RELAY_PIN           18
#define PUMP_RELAY_PIN            5
#define VALVE_RELAY_PIN           17
#define WATER_COOLER_RELAY_PIN    16
#define WATER_HEATER_RELAY_PIN    4
#define PH_LOWERING_RELAY_PIN     27
#define PH_RAISING_RELAY_PIN      15
#define DRAIN_VALVE_RELAY_PIN     12
#define FLUSH_VALVE_RELAY_PIN     14
#define DRAIN_PUMP_RELAY_PIN      13


#define RELAY_SYNC_COOLDOWN       2000UL // Minimum time between relay state changes

// Environmental Thresholds
#define TEMP_ON_THRESHOLD         29.1f // Temperature to turn ON fan
#define TEMP_OFF_THRESHOLD        26.9f // Temperature to turn OFF fan
#define HUMIDITY_MAX_THRESHOLD    70.1f // Humidity to turn ON fan
#define HUMIDITY_MIN_THRESHOLD    60.0f // Humidity to turn OFF fan
#define TEMP_LOW_THRESHOLD        18.0f // Temperature to turn ON heater
#define TEMP_HIGH_THRESHOLD       24.0f // Temperature to turn OFF heater

// Water Temperature (Cooler/Heater)
#define COOLER_ON_TEMP            29.0f // Temperature to turn ON cooler
#define COOLER_OFF_TEMP           27.5f // Temperature to turn OFF cooler
#define HEATER_ON_TEMP            22.0f // Temperature to turn ON heater
#define HEATER_OFF_TEMP           23.5f // Temperature to turn OFF heater

// Pump Cycle
#define PUMP_ON_DURATION          15U // in minutes
#define PUMP_OFF_DURATION         45U // in minutes

// Lighting Schedule
#define MINUTES(h, m)             ((h) * 60 + (m)) 
#define LIGHT_MORNING_ON          MINUTES(5, 30)  // 5:30 AM
#define LIGHT_MORNING_OFF         MINUTES(11, 0)  // 11:00 AM
#define LIGHT_EVENING_ON          MINUTES(15, 0)  // 3:00 PM
#define LIGHT_EVENING_OFF         MINUTES(21, 30) // 9:30 PM

// pH Dosing Control (R385)
#define PH_LOW_THRESHOLD          6.2f // Threshold to turn ON pH lowering
#define PH_LOW_OFF                6.65f // Threshold to turn OFF pH lowering
#define PH_HIGH_THRESHOLD         7.5f // Threshold to turn ON pH raising
#define PH_HIGH_OFF               7.9f // Threshold to turn OFF pH raising
#define PH_PULSE_MS               50UL      // 0.05 seconds
#define PH_PULSE_COUNT            2         // 2 bursts = ~2.5–3 mL total
#define PH_PULSE_GAP_MS           3000UL    // 3 seconds
#define PH_MIN_CHECK_INTERVAL_MS  60000UL // 1 minute
#define PH_REST_PERIOD_MS         300000UL // 5 minutes
#define PH_MAX_DOSING_ATTEMPTS    3 // Max attempts before forcing a rest period

// Drain Pump Control
// === DO Trigger Thresholds ===
#define DO_LOW_THRESHOLD              4.0f       // mg/L emergency
#define DO_LOW_DEBOUNCE_MS            5000UL     // 5s stable low
#define DO_DRAIN_DURATION_MS          (2UL * 60UL * 1000UL)  // 2 minutes
#define DO_COOLDOWN_MS                (30UL * 60UL * 1000UL) // 30 min cooldown
#define MAX_REFILL_MS                 (2UL * 60UL * 1000UL) // 2 min max refill

// Sump Pump Control
#define TURBIDITY_CLEAN_THRESHOLD   400.0f   // NTU level that triggers sump cleaning
#define SUMP_CLEAN_DURATION_MS      30000UL // drain duration

// WiFi Credentials
#define WIFI_SSID                 "meow"
#define WIFI_PASS                 "helloworld2025"