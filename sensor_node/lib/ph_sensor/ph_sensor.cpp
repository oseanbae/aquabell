#include <Arduino.h>
#include <DFRobot_PH.h>
#include "sensor_config.h"

static DFRobot_PH phSensor;

void ph_sensor_init() {
    pinMode(PH_SENSOR_PIN, INPUT);
    phSensor.begin();  // Initialize internal calibration vars
}

float read_ph_voltage_avg() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < NUM_SAMPLES; ++i) {
        sum += analogRead(PH_SENSOR_PIN);
        delay(10);
    }
    return (sum / float(NUM_SAMPLES)) * (VOLTAGE_REF / ADC_MAX);
}

float read_ph(float temperatureCelsius) {
    float voltage = read_ph_voltage_avg();
    return phSensor.readPH(voltage, temperatureCelsius);
}
