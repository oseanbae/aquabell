#ifndef TURBIDITY_SENSOR_H
#define TURBIDITY_SENSOR_H
#include <Arduino.h>

float readAverageVoltage();
float calculateNTU(float voltage);
String classifyTurbidity(float ntu);
void turbidity_sensor_init();
void turbidity_sensor_read();

#endif // TURBIDITY_SENSOR_H