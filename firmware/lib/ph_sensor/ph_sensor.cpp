#include <Arduino.h>
#include "config.h"

// --- Init ---
void ph_sensor_init() {
    pinMode(PH_SENSOR_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // For 0–3.3V input
}

// --- Read trimmed average voltage in mV ---
float read_ph_voltage_avg() {
    float readings[PH_SENSOR_SAMPLES];

    // Sample
    for (int i = 0; i < PH_SENSOR_SAMPLES; i++) {
        int raw = analogRead(PH_SENSOR_PIN);
        readings[i] = raw * ADC_VOLTAGE_MV / ADC_MAX;
        delay(PH_SAMPLE_DELAY_MS);
    }

    // Sort (bubble sort)
    for (int i = 0; i < PH_SENSOR_SAMPLES - 1; i++) {
        for (int j = i + 1; j < PH_SENSOR_SAMPLES; j++) {
            if (readings[j] < readings[i]) {
                float tmp = readings[i];
                readings[i] = readings[j];
                readings[j] = tmp;
            }
        }
    }

    // Trim top/bottom 10% and average
    int trim = PH_SENSOR_SAMPLES / 10;
    float sum = 0.0;
    for (int i = trim; i < PH_SENSOR_SAMPLES - trim; i++) {
        sum += readings[i];
    }

    return sum / (PH_SENSOR_SAMPLES - 2 * trim);
}

// --- Convert voltage to pH ---
float read_ph() {
    float voltage_mV = read_ph_voltage_avg();
    return NEUTRAL_PH + (NEUTRAL_VOLTAGE_MV - voltage_mV) / MV_PER_PH;
}
