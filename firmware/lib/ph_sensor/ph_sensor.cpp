#include "Arduino.h"
#include "config.h"

void ph_sensor_init() {
    pinMode(PH_SENSOR_PIN, INPUT);
}

float read_ph_voltage_avg() {
    long sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(PH_SENSOR_PIN);
        delay(10);
    }
    return ((sum / float(NUM_SAMPLES)) * VOLTAGE_REF) / ADC_MAX;
}

float ph_calibrate() {
    float v = read_ph_voltage_avg() + PH_CALIBRATION_OFFSET;
    return 7.0 + ((2.5 - v) / 0.18);  // pH slope
}
