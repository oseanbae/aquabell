#pragma once

float read_ph_voltage_avg(); // Read average voltage from the pH sensor
void ph_sensor_init(); // Initialize the pH sensor
float read_ph(float waterTempC = NAN); // Read the pH sensor and print the results

