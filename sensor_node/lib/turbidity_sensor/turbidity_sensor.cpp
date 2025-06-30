#include "Arduino.h"
#include "sensor_config.h"

void turbidity_sensor_init() {
    pinMode(TURBIDITY_PIN, INPUT);
    delay(500); // Let voltage stabilize
}

// === Read average ADC voltage (in volts) ===
float readAverageVoltage() {
    long sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(TURBIDITY_PIN);
        delay(5); // Basic filtering
    }
    float avgRaw = sum / float(NUM_SAMPLES);
    return avgRaw * (VOLTAGE_REF / ADC_MAX);  // Returns voltage at ESP32 ADC pin
}

// Read turbidity in NTU (Nephelometric Turbidity Units)
float read_turbidity() {
    float v_adc = readAverageVoltage();               // Voltage at ESP32 pin
    float v_sensor = v_adc * SENSOR_VOLTAGE_GAIN;     // Actual sensor output voltage

    // Empirical nonlinear NTU mapping for SEN0189
    float ntu = -1120.4f * v_sensor * v_sensor + 5742.3f * v_sensor - 4352.9f;
    return max(ntu, 0.0f);                            // Clamp to 0
}


// Classify water quality based on NTU
const char* classifyTurbidity(float ntu) {
    if (ntu <= 50)  return "Clear";               // Optimal system condition
    if (ntu <= 100) return "Slightly Cloudy";     // Still acceptable
    if (ntu <= 200) return "Cloudy";              // Requires monitoring
    if (ntu <= 500) return "Poor";                // Likely filter issues
    return "Very Poor";                           // Critical – urgent attention
}
