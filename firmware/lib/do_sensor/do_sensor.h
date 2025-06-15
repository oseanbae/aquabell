#ifndef DO_SENSOR_H
#define DO_SENSOR_H

#include <Arduino.h>
float readDOVoltage(); 
float calculateDOmgL(float voltage, float temperatureC);
void calibrateDOSinglePoint(float measuredVoltage); // Calibrate the DO sensor with a single point
void do_sensor_init(); // Initialize the DO sensor
void do_sensor_read(); // Read the DO sensor and print the results
void do_sensor_calibrate(); // Calibrate the DO sensor with a single point

#endif