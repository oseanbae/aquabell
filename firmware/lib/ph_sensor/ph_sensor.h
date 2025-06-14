#ifndef PH_SENSOR_H
#define PH_SENSOR_H

#include <Arduino.h>

float read_ph_voltage_avg(int samples = 10); // Read average voltage from the pH sensor
void ph_sensor_init(); // Initialize the pH sensor
void ph_sensor_read(); // Read the pH sensor and print the results

#endif 
