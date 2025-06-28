#pragma once

void do_sensor_init();
float readDOVoltage();
float getDOSaturationUgL(float tempC);
float read_dissolveOxygen(float mV, float tempC);