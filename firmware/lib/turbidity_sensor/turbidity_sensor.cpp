#include "Arduino.h"
#include "config.h"


void turbidity_sensor_init() {
    pinMode(TURBIDITY_PIN, INPUT);
    analogSetAttenuation(ADC_11db);
    delay(500);
}

float readAverageVoltage() {
    long sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(TURBIDITY_PIN);
        delay(5);
    }
    float avgRaw = sum / float(NUM_SAMPLES);
    return avgRaw * (VOLTAGE_REF / ADC_MAX);
}

float voltageToNTU(float v) {
    return max(675.0f * v * v - 3120.0f * v + 3650.0f, 0.0f);
}

float calculateNTU() {
    float v = readAverageVoltage() * 1.5f;
    return voltageToNTU(v);
}

String classifyTurbidity(float ntu) {
    if (ntu <= 70) return "Excellent ✅";
    if (ntu <= 150) return "Good 👍";
    if (ntu <= 300) return "Moderate ⚠️";
    if (ntu <= 500) return "Poor 🚫";
    return "Very Poor ❌";
}
