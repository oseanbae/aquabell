#include <ArduinoJson.h>
#include "mqtt_util.h"

String sensorDataToJson(const RealTimeData& data) {
    StaticJsonDocument<256> doc;

    doc["temp"]         = data.waterTemp;
    doc["ph"]           = data.pH;
    doc["do"]           = data.dissolvedOxygen;
    doc["ntu"]          = data.turbidityNTU;
    doc["airTemp"]      = data.airTemp;
    doc["humidity"]     = data.airHumidity;
    doc["floatSwitch"]  = data.floatTriggered;

    String output;
    serializeJson(doc, output);
    return output;
}
