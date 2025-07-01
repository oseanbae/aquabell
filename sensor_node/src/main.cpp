#include <Arduino.h>
#include "sensor_config.h"

// Sensor libraries
#include "temp_sensor.h"
#include "ph_sensor.h"
#include "do_sensor.h"
#include "turbidity_sensor.h"
#include "dht_sensor.h"
#include "float_switch.h"
#include "sensor_data.h"

// Display libraries
#include "lcd_display.h"

#include "tx_espnow.h"
#include "buffer.h"

unsigned long last_waterTemp_read = 0;
unsigned long last_ph_read = 5000;
unsigned long last_do_read = 10000;
unsigned long last_turbidity_read = 15000;
unsigned long last_DHT_read = 20000;

RealTimeData current; // Use RealTimeData for real-time updates
BatchData sensorBuffer;

unsigned long last_batch_send = 0; // Time to send batch data

void setup() {
    Serial.begin(115200);
    delay(2000); // Let Serial and sensors stabilize
    
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();

    lcd_init(); // Initialize the LCD display
    espnow_init(); // Initialize ESP-NOW for data transmission

    Serial.println("System ready.");
}

void loop() {
    unsigned long now = millis();

    // Water Temp
    if (now - last_waterTemp_read >= DS18B20_READ_INTERVAL) {
        float temp = read_waterTemp();
        if (!isnan(temp)) {
            current.waterTemp = temp;
            sensorBuffer.waterTempSum += temp;
            sensorBuffer.waterTempCount++;

            RealTimeData packet = current;
            packet.isBatch = false;
            sendESPNow(packet);
        }
        last_waterTemp_read = now;
    }

    // pH
    if (now - last_ph_read >= PH_READ_INTERVAL) {
        float ph = read_ph();
        if (!isnan(ph)) {
            current.pH = ph;
            sensorBuffer.pHSum += ph;
            sensorBuffer.pHCount++;

            RealTimeData packet = current;
            packet.isBatch = false;
            sendESPNow(packet);
        }
        last_ph_read = now;
    }

    // Dissolved O2
    if (now - last_do_read >= DO_READ_INTERVAL) {
        float doVoltage = readDOVoltage();
        float dissolveOxygenMgL = read_dissolveOxygen(doVoltage, current.waterTemp);
        if (!isnan(dissolveOxygenMgL)) {
            current.dissolvedOxygen = dissolveOxygenMgL;
            sensorBuffer.doSum += dissolveOxygenMgL;
            sensorBuffer.doCount++;

            RealTimeData packet = current;
            packet.isBatch = false;
            sendESPNow(packet);
        }
        last_do_read = now;
    }

    // Turbidity
    if (now - last_turbidity_read >= TURBIDITY_READ_INTERVAL) {
        float turbidityNTU = read_turbidity();
        if (!isnan(turbidityNTU)) {
            current.turbidityNTU = turbidityNTU;
            sensorBuffer.turbiditySum += turbidityNTU;
            sensorBuffer.turbidityCount++;

            RealTimeData packet = current;
            packet.isBatch = false;
            sendESPNow(packet);
        }
        last_turbidity_read = now;
    }

    // Air Temp & Humidity
    if (now - last_DHT_read >= DHT_READ_INTERVAL) {
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();

        bool updated = false;
        if (!isnan(airTemp)) {
            current.airTemp = airTemp;
            sensorBuffer.airTempSum += airTemp;
            sensorBuffer.airTempCount++;
            updated = true;
        }

        if (!isnan(airHumidity)) {
            current.airHumidity = airHumidity;
            sensorBuffer.airHumiditySum += airHumidity;
            sensorBuffer.airHumidityCount++;
            updated = true;
        }

        if (updated) {
            RealTimeData packet = current;
            packet.isBatch = false;
            sendESPNow(packet);
        }

        last_DHT_read = now;
    }

    // Float switch (continuous)
    current.floatTriggered = float_switch_active();  // true = water low


    // Send 5-minute batch average
    if (now - last_batch_send >= 300000) {
        RealTimeData avg = computeAverages(sensorBuffer);
        avg.isBatch = true;
        sendESPNow(avg);
        resetBatch(sensorBuffer);
        last_batch_send = now;
    }
    
    //lcd_display_update(current);
    yield(); // Keep the system responsive
}
