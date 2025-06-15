#include <Arduino.h>
#include "do_sensor.h"
#include "config.h"

static float V100 = 2.00;  // Set this after calibration (volts)
static const float DO_MAX = 20.0; // mg/L

float readDOVoltage(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; ++i) {
    sum += analogRead(pin);
    delay(5);
  }
  float avg = sum / float(samples);
  return avg * (VOLTAGE_REF / ADC_MAX);
}

float calculateDOmgL(float voltage, float temperatureC) {
  // Linear interpolation from 0 to 100%
  float doPct = constrain(voltage / V100, 0, 1.0);
  float DOmgL = doPct * DO_MAX;
  // Future: add temp compensation curve here (optional)
  return DOmgL;     
}

void calibrateDOSinglePoint(float measuredVoltage) {
  V100 = measuredVoltage;
}
