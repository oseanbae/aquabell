// #include <Arduino.h>
// #include "sensor_config.h"

// // Sensor libraries
// #include "temp_sensor.h"
// #include "ph_sensor.h"
// #include "do_sensor.h"
// #include "turbidity_sensor.h"
// #include "dht_sensor.h"
// #include "float_switch.h"
// #include "sensor_data.h"

// // Display libraries
// #include "lcd_display.h"

// #include "tx_espnow.h"
// #include "buffer.h"

// unsigned long last_waterTemp_read = 0;
// unsigned long last_ph_read = 5000;
// unsigned long last_do_read = 10000;
// unsigned long last_turbidity_read = 15000;
// unsigned long last_DHT_read = 20000;

// RealTimeData current; // Use RealTimeData for real-time updates
// BatchData sensorBuffer;

// unsigned long last_batch_send = 0; // Time to send batch data

// void setup() {
//     Serial.begin(115200);
//     delay(2000); // Let Serial and sensors stabilize
    
//     analogReadResolution(12);
//     analogSetAttenuation(ADC_11db);

//     temp_sensor_init();
//     ph_sensor_init();
//     do_sensor_init();
//     turbidity_sensor_init();
//     dht_sensor_init();
//     float_switch_init();

//     lcd_init(); // Initialize the LCD display
//     espnow_init(); // Initialize ESP-NOW for data transmission

//     Serial.println("System ready.");
// }

// void loop() {
//     unsigned long now = millis();

//     static unsigned long lastRealtimeSend = 0;
//     static unsigned long lastLCDRefresh = 0;
//     static unsigned long lastButtonPress = 0;

//     bool dataChanged = false;

//     // --- Read Sensors ---

//     // Water Temp
//     if (now >= last_waterTemp_read) {
//         float temp = read_waterTemp();
//         if (!isnan(temp)) {
//             current.waterTemp = temp;
//             sensorBuffer.waterTempSum += temp;
//             sensorBuffer.waterTempCount++;
//             dataChanged = true;
//         } else {
//             Serial.println("Error: Water temp read failed.");
//         }
//         last_waterTemp_read += DS18B20_READ_INTERVAL;
//     }

//     // pH
//     if (now >= last_ph_read) {
//         float ph = read_ph();
//         if (!isnan(ph)) {
//             current.pH = constrain(ph, 0.0, 14.0);
//             sensorBuffer.pHSum += current.pH;
//             sensorBuffer.pHCount++;
//             dataChanged = true;
//         } else {
//             Serial.println("Error: pH read failed.");
//         }
//         last_ph_read += PH_READ_INTERVAL;
//     }

//     // Dissolved Oxygen
//     if (now >= last_do_read) {
//         float voltage = readDOVoltage();
//         float doVal = read_dissolveOxygen(voltage, current.waterTemp);
//         if (!isnan(doVal)) {
//             current.dissolvedOxygen = max(doVal, 0.0f);  // Avoid negative values
//             sensorBuffer.doSum += doVal;
//             sensorBuffer.doCount++;
//             dataChanged = true;
//         } else {
//             Serial.println("Error: DO read failed.");
//         }
//         last_do_read += DO_READ_INTERVAL;
//     }

//     // Turbidity
//     if (now >= last_turbidity_read) {
//         float ntu = read_turbidity();
//         if (!isnan(ntu)) {
//             current.turbidityNTU = max(ntu, 0.0f);
//             sensorBuffer.turbiditySum += ntu;
//             sensorBuffer.turbidityCount++;
//             dataChanged = true;
//         } else {
//             Serial.println("Error: Turbidity read failed.");
//         }
//         last_turbidity_read += TURBIDITY_READ_INTERVAL;
//     }

//     // Air Temp / Humidity
//     if (now >= last_DHT_read) {
//         float airTemp = read_dhtTemp();
//         float airHumidity = read_dhtHumidity();
//         bool updated = false;

//         if (!isnan(airTemp)) {
//             current.airTemp = airTemp;
//             sensorBuffer.airTempSum += airTemp;
//             sensorBuffer.airTempCount++;
//             updated = true;
//         } else {
//             Serial.println("Error: DHT temp read failed.");
//         }

//         if (!isnan(airHumidity)) {
//             current.airHumidity = airHumidity;
//             sensorBuffer.airHumiditySum += airHumidity;
//             sensorBuffer.airHumidityCount++;
//             updated = true;
//         } else {
//             Serial.println("Error: DHT humidity read failed.");
//         }

//         if (updated) dataChanged = true;

//         last_DHT_read += DHT_READ_INTERVAL;
//     }

//     // --- Float Switch + Buzzer ---
//     static bool lastFloatState = false;
//     bool floatState = float_switch_active();  // true = water low

//     if (floatState != lastFloatState) {
//         Serial.println(floatState ? "Water LOW - float switch triggered!" : "Water OK");
//         lastFloatState = floatState;
//     }

//     current.floatTriggered = floatState;

//     digitalWrite(LED_PIN, floatState ? HIGH : LOW);
//     digitalWrite(BUZZER_PIN, (floatState && (now / 500) % 2 == 0) ? HIGH : LOW);

//     // --- Real-time ESP-NOW Packet (every 1s max) ---
//     if (now - lastRealtimeSend >= 1000 && dataChanged) {
//         RealTimeData packet = current;
//         packet.isBatch = false;
//         if (!sendESPNow(packet)) {
//             Serial.println("Warning: Real-time ESP-NOW send failed.");
//         }
//         lastRealtimeSend = now;
//     }

//     // --- Batch data every 5 min ---
//     if (now - last_batch_send >= 300000) {
//         RealTimeData avg = computeAverages(sensorBuffer);
//         avg.isBatch = true;

//         if (!sendESPNow(avg)) {
//             Serial.println("Warning: Batch ESP-NOW send failed.");
//         }

//         resetBatch(sensorBuffer);
//         last_batch_send = now;
//     }

//     // --- LCD Display ---
//     lcd_display_update(current);

//     yield(); // Feed the watchdog
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

void setup() {
    Serial.begin(115200);
    delay(2000); // Give time to open Serial Monitor

    // ADC setup
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // Init sensors
    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();

    Serial.println("=== Starting Sensor Monitor ===");
}

void loop() {
    Serial.println("\n--- Sensor Readings ---");

    // Water Temp
    float waterTemp = read_waterTemp();
    if (!isnan(waterTemp)) {
        Serial.print("Water Temp (°C): ");
        Serial.println(waterTemp, 1);
    } else {
        Serial.println("Water Temp: Read Failed");
    }

    // pH
    float ph = read_ph();
    if (!isnan(ph)) {
        Serial.print("pH: ");
        Serial.println(ph, 2);
    } else {
        Serial.println("pH: Read Failed");
    }

    // DO
    float doVoltage = readDOVoltage();
    float dissolvedOxygen = read_dissolveOxygen(doVoltage, waterTemp);
    if (!isnan(dissolvedOxygen)) {
        Serial.print("DO (mg/L): ");
        Serial.println(dissolvedOxygen, 2);
    } else {
        Serial.println("DO: Read Failed");
    }

    // Turbidity
    float turbidity = read_turbidity();
    if (!isnan(turbidity)) {
        Serial.print("Turbidity (NTU): ");
        Serial.println(turbidity, 1);
    } else {
        Serial.println("Turbidity: Read Failed");
    }

    // Air Temp & Humidity
    float airTemp = read_dhtTemp();
    float airHumidity = read_dhtHumidity();

    if (!isnan(airTemp)) {
        Serial.print("Air Temp (°C): ");
        Serial.println(airTemp, 1);
    } else {
        Serial.println("Air Temp: Read Failed");
    }

    if (!isnan(airHumidity)) {
        Serial.print("Air Humidity (%): ");
        Serial.println(airHumidity, 1);
    } else {
        Serial.println("Air Humidity: Read Failed");
    }

    // Float switch
    bool floatTriggered = float_switch_active();
    Serial.print("Water Level: ");
    Serial.println(floatTriggered ? "LOW (Float Triggered)" : "OK");

    Serial.println("-------------------------");

    delay(5000); // Wait 5 seconds before next read
}

