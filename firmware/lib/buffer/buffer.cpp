#include "sensor_data.h"

RealTimeData computeAverages(const BatchData& b) {
    RealTimeData avg;

    if (b.waterTempCount > 0) avg.waterTemp = b.waterTempSum / b.waterTempCount;
    if (b.pHCount > 0) avg.pH = b.pHSum / b.pHCount;
    if (b.doCount> 0) avg.dissolvedOxygen = b.doSum / b.doCount;
    if (b.turbidityCount > 0) avg.turbidityNTU = b.turbiditySum / b.turbidityCount;
    if (b.airTempCount > 0) avg.airTemp = b.airTempSum / b.airTempCount;
    if (b.airHumidityCount > 0) avg.airHumidity = b.airHumiditySum / b.airHumidityCount;

    avg.isBatch = true;
    return avg;
}

void resetBatch(BatchData& b) {
    b.waterTempSum = 0; b.waterTempCount = 0;
    b.pHSum = 0; b.pHCount = 0;
    b.doSum = 0; b.doCount = 0;
    b.turbiditySum = 0; b.turbidityCount = 0;
    b.airTempSum = 0; b.airTempCount = 0;
    b.airHumiditySum = 0; b.airHumidityCount = 0;
}
