#pragma once

void turbidity_sensor_init();
float readAverageVoltage();
float voltageToNTU(float v);
float calculateNTU();
String classifyTurbidity(float ntu);
