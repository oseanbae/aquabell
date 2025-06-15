#ifndef PH_SENSOR_H
#define PH_SENSOR_H

float read_ph_voltage_avg(); // Read average voltage from the pH sensor
void ph_sensor_init(); // Initialize the pH sensor
float ph_calibrate(); // Read the pH sensor and print the results

#endif 
