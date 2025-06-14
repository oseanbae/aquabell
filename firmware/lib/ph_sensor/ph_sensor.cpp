#include <Arduino.h>
#include "config.h"

float read_ph_voltage_avg() {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; ++i) {
    sum += analogRead(PH_SENSOR_PIN);
    delay(10); // Small delay for ADC settle
  }
  return ((sum / (float)NUM_SAMPLES) * VOLTAGE_REF) / ADC_MAX;
}

void ph_sensor_init() {
    pinMode(PH_SENSOR_PIN, INPUT);
}

void ph_sensor_read() {
  int rawADC = analogRead(PH_SENSOR_PIN); //read raw ADC value
  float voltage = read_ph_voltage_avg(); //read average voltage from the pH sensor
  float calibratedVoltage = voltage + PH_CALIBRATION_OFFSET; //apply calibration offset
  float pH = 7.0 + ((2.5 - calibratedVoltage) / 0.18); // Convert voltage to pH using a typical slope of 0.18V/pH unit 

  Serial.print("ADC: ");
  Serial.print(rawADC);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 3);
  Serial.print(" V | Calibrated pH: ");
  Serial.println(pH, 2);

}


