#include <Arduino.h>
#include "config.h"
#include "dht_sensor.h"
#include "temp_sensor.h"
#include "float_switch.h"
#include "turbidity_sensor.h"
#include "ph_sensor.h"

const unsigned long SENSOR_INTERVAL = 3000; // 3 seconds

struct SensorData {
    float airTemp;
    float airHumidity;
    float waterTemp;
    float turbidityNTU;
    float pH;
    bool floatTriggered;
    String turbidityClass;
};

SensorData current;
unsigned long lastSensorRead = 0;

void readSensors() {
    current.airTemp = dht_read_temperature();
    current.airHumidity = dht_read_humidity();
    current.waterTemp = ds18b20_read();
    current.turbidityNTU = calculateNTU();
    current.turbidityClass = classifyTurbidity(current.turbidityNTU);
    current.pH = ph_calibrate();
    current.floatTriggered = float_switch_triggered();

#ifdef ENABLE_LOGGING
    Serial.println("----- Sensor Readings -----");
    Serial.print("Air Temp: "); Serial.println(current.airTemp);
    Serial.print("Humidity: "); Serial.println(current.airHumidity);
    Serial.print("Water Temp: "); Serial.println(current.waterTemp);
    Serial.print("Turbidity (NTU): "); Serial.println(current.turbidityNTU);
    Serial.print("Turbidity Quality: "); Serial.println(current.turbidityClass);
    Serial.print("pH: "); Serial.println(current.pH);
    if (current.floatTriggered) Serial.println("⚠️ Float switch just triggered!");
    Serial.println("---------------------------");
#endif
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Let Serial and sensors stabilize

    dht_sensor_init();
    temp_sensor_init();
    float_switch_init();
    turbidity_sensor_init();
    ph_sensor_init();

    Serial.println("System ready.");
}

void loop() {
    if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
        lastSensorRead = millis();
        readSensors();
    }
}
