#pragma once 
#include <Arduino.h>
#include "sensor_data.h"

RealTimeData computeAverages(const BatchData& batch);
void resetBatch(BatchData& batch);
