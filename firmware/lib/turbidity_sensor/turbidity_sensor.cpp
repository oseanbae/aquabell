#include "Arduino.h"
#include "config.h"


void turbidity_sensor_init() {
    pinMode(TURBIDITY_PIN, INPUT);
    delay(500); // Let voltage stabilize
}

// === Read average ADC voltage (in volts) ===
float readTurbidityVoltage() {
    long sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(TURBIDITY_PIN);
        delay(5); // Simple noise filtering
    }
    float avgRaw = sum / float(NUM_SAMPLES);
    return avgRaw * (VOLTAGE_REF / ADC_MAX);  // Voltage at ESP32 ADC pin
}

// === Convert voltage to NTU ===
float read_turbidity() {
    float v_adc = readTurbidityVoltage();           // Voltage at ESP32 ADC pin
    float v_sensor = v_adc * SENSOR_VOLTAGE_GAIN;   // Actual sensor output voltage
    float ntu = NTU_OFFSET + NTU_SLOPE * v_sensor;  // Linear mapping model
    return max(ntu, 0.0f);                          // Prevent negatives
}

// === Interpret turbidity level ===
const char* classifyTurbidity(float ntu) {
    if (ntu <= 50.0f)   return "Clear";
    if (ntu <= 100.0f)  return "Slightly Cloudy";
    if (ntu <= 200.0f)  return "Cloudy";
    if (ntu <= 500.0f)  return "Poor";
    return "Very Poor";
}
