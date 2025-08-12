// #include <Arduino.h>
// #include <Wifi.h>
// #include <RTClib.h>

// #include "config.h"
// #include "temp_sensor.h"
// #include "ph_sensor.h"
// #include "do_sensor.h"
// #include "turbidity_sensor.h"
// #include "dht_sensor.h"
// #include "float_switch.h"
// #include "sensor_data.h"
// #include "rule_engine.h"
// #include "lcd_display.h"
// #include "firestore_client.h"

// // === CONSTANTS ===
// #define USE_MOCK_DATA true

// RTC_DS3231 rtc;
// RealTimeData current;


// // void connectWiFi() {
// //     WiFi.begin(WIFI_SSID, WIFI_PASS);
// //     Serial.print("Connecting to WiFi...");
// //     while (WiFi.status() != WL_CONNECTED) {
// //         delay(500);
// //         Serial.print(".");
// //     }
// //     Serial.println(" connected.");
// // }

// // === FORWARD DECLARATIONS ===
// void initAllModules();
// RealTimeData readSensors(unsigned long now);
// RealTimeData readMockSensors(unsigned long now);
// void displaySensorData(const RealTimeData& data);

// // === SETUP ===
// void setup() {
//     Serial.begin(115200);
//     delay(1000);

//     analogReadResolution(12);
//     analogSetAttenuation(ADC_11db);

//     while (!rtc.begin()) {
//         Serial.println("❌ RTC not found. Retrying...");
//         delay(3000);
//     }

//     if (rtc.lostPower()) {
//         rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//         Serial.println("⚠️ RTC reset to compile time.");
//     }

//     initAllModules();
//     //connectWiFi();

//     // >>> Sign-in to Firebase on boot
//     //firebaseSignIn();
// }

// unsigned long lastDisplay = 0;
// const unsigned long displayInterval = 2000; // 2 seconds

// // === LOOP ===
// void loop() {
//     // if (WiFi.status() != WL_CONNECTED) {
//     //     connectWiFi();
//     // }

//     unsigned long nowMillis = millis();

//     DateTime now = rtc.now();
    
//     // static unsigned long lastLiveUpdate = 0;
//     // static unsigned long lastBatchLog = 0;
//     // const unsigned long liveInterval = 10000;
//     // const unsigned long batchInterval = 600000;
//     // const int batchSize = 60;
//     // static RealTimeData logBuffer[batchSize];
//     // static int logIndex = 0;

//     // Read new sensor values (or mock if enabled)
//     current = USE_MOCK_DATA ? readMockSensors(nowMillis) : readSensors(nowMillis);

//     Serial.println("Current time: " + String(now.timestamp()));


//     // if (nowMillis - lastLiveUpdate >= liveInterval) {
//     //     pushToFirestoreLive(current);
//     //     logBuffer[logIndex++] = current;
//     //     lastLiveUpdate = nowMillis;
//     // }


//     // if ((nowMillis - lastBatchLog >= batchInterval) || (logIndex >= batchSize)) {
//     //     pushBatchLogToFirestore(logBuffer, logIndex);
//     //     logIndex = 0;
//     //     lastBatchLog = nowMillis;
//     // }

    
//     // lcd_display_update(current); 

//     yield();  // Feed watchdog
// }

// // === INIT HELPERS ===
// void initAllModules() {
//     temp_sensor_init();
//     ph_sensor_init();
//     do_sensor_init();
//     turbidity_sensor_init();
//     dht_sensor_init();
//     float_switch_init();
//     lcd_init();
//     Serial.println("✅ All modules initialized successfully.");
// }

// // === SENSOR READ FUNCTION ===
// RealTimeData readSensors(unsigned long now) {
//     static unsigned long lastTempRead = 0, lastPHRead = 0, lastDORead = 0;
//     static unsigned long lastTurbidityRead = 0, lastDHTRead = 0, lastFloatRead = 0;

//     RealTimeData data = current;

//     if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
//         float temp = read_waterTemp();
//         if (!isnan(temp)) {
//             data.waterTemp = temp;
//             Serial.print("Water Temp: ");
//             Serial.println(temp);
//         }
//         lastTempRead = now;
//     }

//     if (now - lastPHRead >= PH_READ_INTERVAL) {
//         float ph = read_ph();
//         if (!isnan(ph)) {
//             data.pH = constrain(ph, 0.0, 14.0);
//             Serial.print("pH: ");
//             Serial.println(data.pH);
//         }
//         lastPHRead = now;
//     }

//     if (now - lastDORead >= DO_READ_INTERVAL) {
//         float voltage = readDOVoltage();
//         float doVal = read_dissolveOxygen(voltage, data.waterTemp);
//         if (!isnan(doVal)) {
//             data.dissolvedOxygen = max(doVal, 0.0f);
//             Serial.print("Dissolved Oxygen: ");
//             Serial.println(data.dissolvedOxygen);
//         }
//         lastDORead = now;
//     }

//     if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
//         float ntu = read_turbidity();
//         if (!isnan(ntu)) {
//             data.turbidityNTU = max(ntu, 0.0f);
//             Serial.print("Turbidity (NTU): ");
//             Serial.println(data.turbidityNTU);
//         }
//         lastTurbidityRead = now;
//     }

//     if (now - lastDHTRead >= DHT_READ_INTERVAL) {
//         float airTemp = read_dhtTemp();
//         float airHumidity = read_dhtHumidity();
//         if (!isnan(airTemp)) {
//             data.airTemp = airTemp;
//             Serial.print("Air Temp: ");
//             Serial.println(airTemp);
//         }
//         if (!isnan(airHumidity)) {
//             data.airHumidity = airHumidity;
//             Serial.print("Air Humidity: ");
//             Serial.println(airHumidity);
//         }
//         lastDHTRead = now;
//     }

//     if (now - lastFloatRead >= FLOAT_READ_INTERVAL) {
//         lastFloatRead = now;
//         static bool lastFloatState = false;
//         bool floatState = float_switch_active();
//         if (floatState != lastFloatState) {
//             data.floatTriggered = floatState;
//             Serial.print("Float Switch Triggered: ");
//             Serial.println(floatState ? "YES" : "NO");
//             lastFloatState = floatState;
//         }
//     }

//     return data;
// }

// // === MOCK SENSOR DATA ===
// RealTimeData readMockSensors(unsigned long now) {
//     RealTimeData currentMock;
//     currentMock.waterTemp = 25.0 + (rand() % 100) / 10.0; // Random temp between 25.0 and 26.0
//     currentMock.pH = 7.0 + (rand() % 100) / 100.0; // Random pH between 7.0 and 8.0
//     currentMock.dissolvedOxygen = 5.0 + (rand() % 100) / 100.0; // Random DO between 5.0 and 6.0
//     currentMock.turbidityNTU = (rand() % 100) / 10.0; // Random turbidity between 0.0 and 10.0 NTU
//     currentMock.airTemp = 20.0 + (rand() % 100) / 10.0; // Random air temp between 20.0 and 21.0
//     currentMock.airHumidity = 50.0 + (rand() % 100) / 10.0; // Random humidity between 50.0 and 51.0
//     currentMock.floatTriggered = (rand() % 2) == 0; // Randomly triggered or not

//     return currentMock;
// }

// void displaySensorData(const RealTimeData& data) {
//     Serial.println("=== Sensor Data ===");
//     Serial.printf("Water Temp: %.2f °C\n", data.waterTemp);
//     Serial.printf("pH: %.2f\n", data.pH);
//     Serial.printf("Dissolved Oxygen: %.2f mg/L\n", data.dissolvedOxygen);
//     Serial.printf("Turbidity (NTU): %.2f\n", data.turbidityNTU);
//     Serial.printf("Air Temp: %.2f °C\n", data.airTemp);
//     Serial.printf("Air Humidity: %.2f %%\n", data.airHumidity);
//     Serial.printf("Float Triggered: %s\n", data.floatTriggered ? "YES" : "NO");
// }

#include <Arduino.h>
#include <WiFi.h>
#include <RTClib.h>

#include "config.h"
#include "temp_sensor.h"
#include "ph_sensor.h"
#include "do_sensor.h"
#include "turbidity_sensor.h"
#include "dht_sensor.h"
#include "float_switch.h"
#include "sensor_data.h"
#include "rule_engine.h"
#include "lcd_display.h"
#include "firestore_client.h"

// === CONSTANTS ===
#define USE_MOCK_DATA true

RTC_DS3231 rtc;
RealTimeData current;

// === FORWARD DECLARATIONS ===
void initAllModules();
bool readSensors(unsigned long now, RealTimeData &data);

// === SETUP ===
void setup() {
    Serial.begin(115200);
    delay(1000);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    while (!rtc.begin()) {
        Serial.println("❌ RTC not found. Retrying...");
        delay(3000);
    }

    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        Serial.println("⚠️ RTC reset to compile time.");
    }

    initAllModules();
}

// === LOOP ===
unsigned long lastRuleCheck = 0;

void loop() {
    unsigned long nowMillis = millis();

    bool updated = readSensors(nowMillis, current);
    if (updated || is_float_switch_triggered()) {
        apply_rules(current, rtc.now());
    }

    yield();
}

// === INIT HELPERS ===
void initAllModules() {
    temp_sensor_init();
    ph_sensor_init();
    do_sensor_init();
    turbidity_sensor_init();
    dht_sensor_init();
    float_switch_init();
    lcd_init();
    Serial.println("✅ All modules initialized successfully.");
}

// === SENSOR READ FUNCTION ===
bool readSensors(unsigned long now, RealTimeData &data) {
    static unsigned long lastTempRead = 0, lastPHRead = 0, lastDORead = 0;
    static unsigned long lastTurbidityRead = 0, lastDHTRead = 0, lastFloatRead = 0;
    static bool lastFloatState = false;

    bool updated = false;
    data = current;

    // --- Water Temp ---
    if (now - lastTempRead >= DS18B20_READ_INTERVAL) {
        data.waterTemp = USE_MOCK_DATA ? 
            25.0 + (rand() % 100) / 10.0 : read_waterTemp();
        lastTempRead = now;
        updated = true;
    }

    // --- pH ---
    if (now - lastPHRead >= PH_READ_INTERVAL) {
        data.pH = USE_MOCK_DATA ? 
            6.5 + (rand() % 50) / 10.0 : read_ph();
        lastPHRead = now;
        updated = true;
    }

    // --- Dissolved Oxygen ---
    if (now - lastDORead >= DO_READ_INTERVAL) {
        data.dissolvedOxygen = USE_MOCK_DATA ? 
            5.0 + (rand() % 40) / 10.0 : read_dissolveOxygen(readDOVoltage(), data.waterTemp);
        lastDORead = now;
        updated = true;
    }

    // --- Turbidity ---
    if (now - lastTurbidityRead >= TURBIDITY_READ_INTERVAL) {
        data.turbidityNTU = USE_MOCK_DATA ? 
            (rand() % 1000) / 10.0 : read_turbidity();
        lastTurbidityRead = now;
        updated = true;
    }

    // --- DHT ---
    if (now - lastDHTRead >= DHT_READ_INTERVAL) {
        if (USE_MOCK_DATA) {
            data.airTemp = 20.0 + (rand() % 150) / 10.0;
            data.airHumidity = 40.0 + (rand() % 600) / 10.0;
        } else {
            data.airTemp = read_dhtTemp();
            data.airHumidity = read_dhtHumidity();
        }
        lastDHTRead = now;
        updated = true;
    }

    // --- Float Switch ---
    if (now - lastFloatRead >= FLOAT_READ_INTERVAL) {
        bool floatState = USE_MOCK_DATA ? (rand() % 2) : float_switch_active();
        if (floatState != lastFloatState) {
            data.floatTriggered = floatState;
            lastFloatState = floatState;
            updated = true;
        }
        lastFloatRead = now;
    }

    return updated;
}
