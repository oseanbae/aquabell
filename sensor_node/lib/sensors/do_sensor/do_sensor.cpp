#include "do_sensor.h"
#include "sensor_config.h"
#include <Arduino.h>

// DO saturation lookup table (µg/L at 0–40°C)
static const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410
};
void do_sensor_init() {
  pinMode(DO_SENSOR_PIN, INPUT);
  analogReadResolution(12); // ESP32 ADC resolution
  analogSetAttenuation(ADC_11db); // Set ADC attenuation to allow full range (0-3.3V)
  Serial.println("DO Sensor initialized.");
}
// Reads the DO sensor voltage in mV
float readDOVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(DO_SENSOR_PIN);
    delay(5);
  }
  float raw = sum / float(NUM_SAMPLES);
  return (raw * 3300.0) / 4095.0;  // Convert ADC to mV
}

float getDOSaturationUgL(float tempC) {
  if (tempC <= 0) return DO_Table[0];
  if (tempC >= 40) return DO_Table[40];
  int t = int(tempC);
  float frac = tempC - t;
  return DO_Table[t] + (DO_Table[t + 1] - DO_Table[t]) * frac;
}

float read_dissolveOxygen(float mV, float tempC) {
  float sat_now = getDOSaturationUgL(tempC) / 1000.0;
  float sat_cal = getDOSaturationUgL(DO_CAL_TEMP) / 1000.0;
  return (mV / DO_CAL_VOLTAGE) * (sat_now / sat_cal);
}
