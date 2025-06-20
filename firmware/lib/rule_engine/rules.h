#pragma once
#include "sensor_data.h" // you'll define this shared struct
#include "config.h" // you'll define this shared config
#include "RTClib.h" // for DateTime

void apply_rules(const SensorData& data, const DateTime& now);
void checkHumidityAlert(float humidity, float airTemp, int currentMinutes);