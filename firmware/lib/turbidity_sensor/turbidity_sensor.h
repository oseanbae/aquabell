#pragma once

void turbidity_sensor_init();
float read_turbidity();
const char* classifyTurbidity(float ntu);