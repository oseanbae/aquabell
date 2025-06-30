#include <Arduino.h>
#include "sensor_config.h"

void ph_sensor_init() {
    pinMode(PH_SENSOR_PIN, INPUT);
}

// Read the average voltage from the pH sensor
float read_ph_voltage_avg() {
    uint32_t sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum += analogRead(PH_SENSOR_PIN);
        delay(10);
    }
    float raw = sum / float(NUM_SAMPLES);
    return raw * VOLTAGE_REF / ADC_MAX;
}

// Calculate pH from voltage in mV using the Nernst equation
float read_ph() {
    float voltage_mV = read_ph_voltage_avg();
    return NEUTRAL_PH + (NEUTRAL_VOLTAGE_MV - voltage_mV) / MV_PER_PH;
}   