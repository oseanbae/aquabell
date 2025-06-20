#include "do_sensor.h"
#include "config.h"
#include <Arduino.h>

// DO saturation lookup table (µg/L at 0–40°C)
static const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410
};

// Reads the DO sensor voltage in mV
float readDOVoltage() {
    uint32_t sum = 0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(DO_SENSOR_PIN);
        delay(5);
    }
    float avg = sum / float(NUM_SAMPLES);
    return avg * (3300.0 / 4095.0);  // 3.3V ref on ESP32, 12-bit ADC
}

// Gets the expected DO saturation (µg/L) for a given temperature
float getDOSaturationUgL(float tempC) {
    if (tempC <= 0) return DO_Table[0];
    if (tempC >= 40) return DO_Table[40];
    int t = (int)tempC;
    float f = tempC - t;
    return DO_Table[t] + (DO_Table[t + 1] - DO_Table[t]) * f;
}

// Calculates DO in mg/L based on measured voltage and current temperature
float calculateDOMgL(float measured_mV, float tempC) {
    float satAtCurrent = getDOSaturationUgL(tempC);       // µg/L now
    float satAtCal     = getDOSaturationUgL(DO_CAL_TEMP); // µg/L during calibration
    return (measured_mV / DO_CAL_VOLTAGE) * (satAtCurrent / 1000.0); // mg/L
}
