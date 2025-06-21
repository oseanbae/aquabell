#include "Arduino.h"
#include "config.h"

void turbidity_sensor_init() {
    pinMode(TURBIDITY_PIN, INPUT);
    analogSetAttenuation(ADC_11db);  // Allow full 0–3.3V range on ESP32 ADC
    delay(500);
}

// Average ADC voltage over multiple samples
float readAverageVoltage() {
    long sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(TURBIDITY_PIN);
        delay(5);
    }

    float avgRaw = sum / float(NUM_SAMPLES);
    return avgRaw * (VOLTAGE_REF / ADC_MAX);  // Return ESP32 ADC pin voltage (0–3.3V)
}

// Calculate NTU using linear formula
float calculateNTU() {
    float v_esp32 = readAverageVoltage();     // Voltage at ESP32 ADC pin (0–3.0V max)
    float ntu = 1125.0 - 375.0 * v_esp32;     // Linear NTU mapping
    return max(ntu, 0.0f);                    // Clamp to 0 if negative
}

// Classify water quality based on NTU
const char* classifyTurbidity(float ntu) {
    if (ntu <= 50)  return "Clear";               // Optimal system condition
    if (ntu <= 100) return "Slightly Cloudy";     // Still acceptable
    if (ntu <= 200) return "Cloudy";              // Requires monitoring
    if (ntu <= 500) return "Poor";                // Likely filter issues
    return "Very Poor";                           // Critical – urgent attention
}
