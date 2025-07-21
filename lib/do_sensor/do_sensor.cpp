#include "do_sensor.h"
#include "config.h"
#include "helper.h"
#include <Arduino.h>

// DO saturation lookup table (µg/L at 0–40°C)
// DFRobot's SEN0244 sensor uses a linear interpolation based on temperature.
// The values are based on the saturation of oxygen in water at different temperatures.
static const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410
};

void do_sensor_init() {
  pinMode(DO_SENSOR_PIN, INPUT);
}

// Reads the DO sensor voltage in mV
float readDOVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < DO_SENSOR_SAMPLES; i++) {
    sum += analogRead(DO_SENSOR_PIN);
    delay(5);
  }
  float raw = sum / float(DO_SENSOR_SAMPLES);
  return (raw * ADC_VOLTAGE_MV) / ADC_MAX;  // Convert ADC to mV
}

// Converts temperature in °C to DO saturation in µg/L using a linear interpolation
float getDOSaturationUgL(float tempC) {
  if (tempC <= 0) return DO_Table[0]; 
  if (tempC >= 40) return DO_Table[40];
  int t = int(tempC);
  float frac = tempC - t;
  return DO_Table[t] + (DO_Table[t + 1] - DO_Table[t]) * frac;
}

// Reads the DO concentration in µg/L based on the sensor voltage and temperature
float read_dissolveOxygen(float mV, float tempC) {
  float sat_now = getDOSaturationUgL(tempC) / 1000.0;
  float sat_cal = getDOSaturationUgL(DO_CAL_TEMP) / 1000.0;
  return (mV / DO_CAL_VOLTAGE) * (sat_now / sat_cal);
}
