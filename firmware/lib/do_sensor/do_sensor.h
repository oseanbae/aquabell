#pragma once

void do_sensor_init();
float readDOVoltage();
float getDOSaturationUgL(float tempC);
float calculateDOMgL(float mV, float tempC);