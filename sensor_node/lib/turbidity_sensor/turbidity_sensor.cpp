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
        delay(5); // Basic filtering
    }
    float avgRaw = sum / float(NUM_SAMPLES);
    return avgRaw * (VOLTAGE_REF / ADC_MAX);  // Returns voltage at ESP32 ADC pin
}

// === Convert voltage to NTU ===
float read_turbidity() {
    float v_adc = readTurbidityVoltage();                // Voltage at ESP32 pin (0–3.0V)
    float v_sensor = v_adc * SENSOR_VOLTAGE_GAIN;      // Actual sensor output (0–4.5V)
    float ntu = NTU_OFFSET + NTU_SLOPE * v_sensor;     // Linear mapping
    return max(ntu, 0.0f);                              // Clamp negative values
}

// Classify water quality based on NTU
const char* classifyTurbidity(float ntu) {
    if (ntu <= 50)  return "Clear";               // Optimal system condition
    if (ntu <= 100) return "Slightly Cloudy";     // Still acceptable
    if (ntu <= 200) return "Cloudy";              // Requires monitoring
    if (ntu <= 500) return "Poor";                // Likely filter issues
    return "Very Poor";                           // Critical – urgent attention
}
