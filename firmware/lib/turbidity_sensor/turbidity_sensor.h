#pragma once

void turbidity_sensor_init();
float calculateNTU();
const char* classifyTurbidity(float ntu);