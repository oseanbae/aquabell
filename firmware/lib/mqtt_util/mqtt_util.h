#pragma once

#include <ArduinoJson.h>
#include "sensor_data.h"

String sensorDataToJson(const RealTimeData& data);
