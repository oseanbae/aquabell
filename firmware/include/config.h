#ifndef CONFIG_H
#define CONFIG_H

//Sensor Configuration for ESP32
#define ADC_MAX 4095.0 // Maximum ADC value for ESP32 (12-bit resolution)
#define VOLTAGE_REF 3.3 // Reference voltage for ADC (ESP32 typically uses 3.3V)

//DHT Sensor Configuration
#define DHT_PIN 22 // GPIO pin connected to the DHT sensor
#define DHT_TYPE DHT11 // Type of DHT sensor (DHT11, DHT22, etc.)

//DS18B20 Temperature Sensor Configuration
#define ONE_WIRE_BUS 23 // GPIO pin for OneWire bus (if using OneWire sensors)

//TURBIDITY Sensor Configuration
#define TURBIDITY_PIN 36 // GPIO pin connected to the turbidity sensor
#define NUM_SAMPLES 10 // Number of samples to average for turbidity sensor
#define R1 10000.0f     // Resistor values for voltage divider (R1 = 10k, R2 = 20k)
#define R2 20000.0f     // Resistor values for voltage divider (R1 = 10k, R2 = 20k)
#define SENSOR_VOLTAGE_GAIN ((R1 + R2) / (R2)) // Voltage divider gain for the turbidity sensor
// Optional: Threshold to detect voltage near ESP32 ADC max input
#define MAX_ADC_SAFE_VOLTAGE 3.1 // Warn if voltage exceeds this (optional)

//Float Switch Configuration
#define FLOAT_SWITCH_PIN 25 // GPIO pin connected to the float switch

//PH Sensor Configuration
#define PH_SENSOR_PIN 39 // GPIO pin connected to the pH sensor
#define PH_CALIBRATION_OFFSET 0.0 // Calibration offset for pH sensor (adjust as needed)

#endif
