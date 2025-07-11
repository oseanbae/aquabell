// #include <Arduino.h>
// #include "sensor_config.h"
// #include "lcd_display.h"
// #include "sensor_data.h"

// RealTimeData mockData;

// void setup() {
//     Serial.begin(115200);
//     delay(1000);

//     lcd_init();

//     // Assign mock values manually
//     mockData.waterTemp = 26.5;
//     mockData.pH = 7.2;
//     mockData.dissolvedOxygen = 6.8;
//     mockData.turbidityNTU = 45.0;
//     mockData.airTemp = 24.3;
//     mockData.airHumidity = 58.0;
//     mockData.floatTriggered = 0;
//     mockData.isBatch = 0;

//     Serial.println("LCD Display Test Initialized.");
// }


// void loop() {
//     lcd_display_update(mockData);
//     delay(200);  // Let loop breathe, simulate frame refresh
// }


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

RealTimeData current; // Use RealTimeData for real-time updates
BatchData sensorBuffer;

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

    lcd_init();
    espnow_init();

    Serial.println("System ready.");
}

void loop() {
    unsigned long now = millis();

    static unsigned long lastRealtimeSend = 0;
    static unsigned long last_batch_send = 0;

    // Sensor read timestamps
    static unsigned long last_waterTemp_read = 0;
    static unsigned long last_ph_read = 0;
    static unsigned long last_do_read = 0;
    static unsigned long last_turbidity_read = 0;
    static unsigned long last_DHT_read = 0;

    // Flags to track updates
    bool waterTempUpdated = false;
    bool phUpdated = false;
    bool doUpdated = false;
    bool turbidityUpdated = false;
    bool dhtUpdated = false;

    // --- Read Sensors ---

    // Water Temp
    if (now >= last_waterTemp_read) {
        float temp = read_waterTemp();
        if (!isnan(temp)) {
            current.waterTemp = temp;
            sensorBuffer.waterTempSum += temp;
            sensorBuffer.waterTempCount++;
            waterTempUpdated = true;
        }
        last_waterTemp_read += DS18B20_READ_INTERVAL;
    }

    // pH
    if (now >= last_ph_read) {
        float ph = read_ph();
        if (!isnan(ph)) {
            current.pH = constrain(ph, 0.0, 14.0);
            sensorBuffer.pHSum += current.pH;
            sensorBuffer.pHCount++;
            phUpdated = true;
        }
        last_ph_read += PH_READ_INTERVAL;
    }

    // Dissolved Oxygen
    if (now >= last_do_read) {
        float voltage = readDOVoltage();
        float doVal = read_dissolveOxygen(voltage, current.waterTemp);
        if (!isnan(doVal)) {
            current.dissolvedOxygen = max(doVal, 0.0f);
            sensorBuffer.doSum += doVal;
            sensorBuffer.doCount++;
            doUpdated = true;
        }
        last_do_read += DO_READ_INTERVAL;
    }

    // Turbidity
    if (now >= last_turbidity_read) {
        float ntu = read_turbidity();
        if (!isnan(ntu)) {
            current.turbidityNTU = max(ntu, 0.0f);
            sensorBuffer.turbiditySum += ntu;
            sensorBuffer.turbidityCount++;
            turbidityUpdated = true;
        }
        last_turbidity_read += TURBIDITY_READ_INTERVAL;
    }

    // DHT (Air Temp / Humidity)
    if (now >= last_DHT_read) {
        bool updated = false;
        float airTemp = read_dhtTemp();
        float airHumidity = read_dhtHumidity();

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

        if (updated) dhtUpdated = true;

        last_DHT_read += DHT_READ_INTERVAL;
    }

    // --- Float Switch ---
    static bool lastFloatState = false;
    static unsigned long lastFloatDebounce = 0;

    if (now - lastFloatDebounce >= 100) {
        bool floatState = float_switch_active();
        if (floatState != lastFloatState) {
            lastFloatState = floatState;
            current.floatTriggered = floatState;
            Serial.println(floatState ? "Water LOW" : "Water OK");
        }
        digitalWrite(LED_PIN, floatState ? HIGH : LOW);
        lastFloatDebounce = now;
    }

    // --- Real-time ESP-NOW send ---
    // bool anySensorUpdated = waterTempUpdated || phUpdated || doUpdated || turbidityUpdated || dhtUpdated;

    // if (anySensorUpdated && (now - lastRealtimeSend >= 1000)) {
    //     current.isBatch = false;

    //     Serial.print("📶 Real-time send triggered by: ");
    //     if (waterTempUpdated) Serial.print("[WaterTemp] ");
    //     if (phUpdated) Serial.print("[pH] ");
    //     if (doUpdated) Serial.print("[DO] ");
    //     if (turbidityUpdated) Serial.print("[Turbidity] ");
    //     if (dhtUpdated) Serial.print("[DHT] ");
    //     Serial.println();

    //     Serial.println("===Sending Sensor Values:===");
    //     Serial.printf("Water Temp      : %.2f °C\n", current.waterTemp);
    //     Serial.printf("pH Level        : %.2f\n", current.pH);
    //     Serial.printf("Diss. Oxygen    : %.2f mg/L\n", current.dissolvedOxygen);
    //     Serial.printf("Turbidity       : %.2f NTU\n", current.turbidityNTU);
    //     Serial.printf("Air Temp        : %.2f °C\n", current.airTemp);
    //     Serial.printf("Air Humidity    : %.2f %%\n", current.airHumidity);
    //     Serial.printf("Float Triggered : %s\n", current.floatTriggered ? "YES" : "NO");

    //     if (!sendESPNow(current)) {
    //         Serial.println("❌ ESP-NOW real-time send failed.");
    //     } else {
    //         Serial.println("✅ ESP-NOW real-time send successful.");
    //     }

    //     lastRealtimeSend = now;
    // }


    // // --- Batch ESP-NOW send every 5 minutes ---
    // if (now - last_batch_send >= BATCH_SEND_INTERVAL) {
    //     RealTimeData avg = computeAverages(sensorBuffer);
    //     avg.isBatch = true;
    //     avg.floatTriggered = current.floatTriggered;

    //     if (!sendESPNow(avg)) {
    //         Serial.println("⚠️ ESP-NOW batch send failed.");
    //     } else {
    //         Serial.println("📤 Batch data sent.");
    //     }

    //     resetBatch(sensorBuffer);
    //     last_batch_send = now;
    // }

    // --- LCD Display ---
    lcd_display_update(current);

    yield(); // Feed the watchdog
}