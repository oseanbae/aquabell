#include <Arduino.h>
#include "config.h"

// Derived from calibration
float ph_slope = 0.0;
float ph_intercept = 0.0;

// --- Init ---
void ph_sensor_init() {
    pinMode(PH_SENSOR_PIN, INPUT);
    analogReadResolution(12);           // 12-bit ADC: 0–4095
    analogSetAttenuation(ADC_11db);      // For 0–3.3V input range

    // Calculate slope & intercept from calibration data
    ph_slope = (CAL_PH2 - CAL_PH1) / (CAL_VOLTAGE2_MV - CAL_VOLTAGE1_MV);
    ph_intercept = CAL_PH1 - (ph_slope * CAL_VOLTAGE1_MV);
}

// --- Read trimmed average voltage in mV ---
float read_ph_voltage_avg() {
    float readings[PH_SENSOR_SAMPLES];

    for (int i = 0; i < PH_SENSOR_SAMPLES; i++) {
        int raw = analogRead(PH_SENSOR_PIN);
        readings[i] = raw * 3300.0 / 4095.0; // Convert to mV
        delay(PH_SAMPLE_DELAY_MS);
    }

    // Sort
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
float read_ph(float waterTempC = NAN) {
    float voltage_mV = read_ph_voltage_avg();
    float ph = ph_slope * voltage_mV + ph_intercept;

    // If water temperature is available, apply Nernst-based compensation
    if (!isnan(waterTempC)) {
        // Ideal slope at 25 °C
        const float SLOPE_25C = 59.16; // mV/pH
        // Ideal slope at measured temp
        float slope_T = SLOPE_25C * ((waterTempC + 273.15) / 298.15);

        // Correction factor relative to 25 °C
        float correction = SLOPE_25C / slope_T;

        // Apply correction
        ph = (ph - 7.0) * correction + 7.0;
    }

    return ph;
}

