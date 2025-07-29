#include <Arduino.h>
#include <unity.h>

#include "rule_engine.cpp"  // INCLUDE THE FULL IMPLEMENTATION
#include "sensor_data.h"
#include "test_mocks.h"

// === GLOBAL MOCK CONTROL ===
unsigned long mockMillis = 0;
bool mockFloatSwitchState = false;

// Mock millis() function
unsigned long mockMillisFunc() {
    return mockMillis;
}

void setUp() { }
void tearDown() { }

// --- TEST: Fan turns OFF after runtime limit exceeded ---
void test_fan_runtime_limit() {
    RealTimeData testData = {};
    testData.airTemp = 29.0f;
    testData.airHumidity = 90.0f;

    int currentMinutes = 600;  // 10:00 AM

    // Simulate Fan turning ON initially
    mockMillis = 0;
    check_climate_and_control_fan(testData.airTemp, testData.airHumidity, currentMinutes);

    // Fast-forward time by FAN_MAX_CONTINUOUS_MS + extra minute to exceed runtime
    mockMillis += FAN_MAX_CONTINUOUS_MS + 60000;

    // Should trigger Fan OFF
    check_climate_and_control_fan(testData.airTemp, testData.airHumidity, currentMinutes);
}

// --- TEST: Pump cycles ON/OFF based on schedule ---
void test_pump_schedule_cycle() {
    float waterTemp = 25.0f;
    mockFloatSwitchState = false;

    // Simulate initial time: Pump OFF phase just ended
    mockMillis = PUMP_OFF_DURATION + 1000;

    // Should turn pump ON
    check_and_control_pump(waterTemp, mockFloatSwitchState);

    // Fast-forward time to simulate pump ON duration exceeded
    mockMillis += PUMP_ON_DURATION + 1000;

    // Should turn pump OFF
    check_and_control_pump(waterTemp, mockFloatSwitchState);
}

// --- TEST: Light ON/OFF Schedules ---
void test_light_schedule_logic() {
    // Light ON in morning window
    DateTime fakeNow(2024, 7, 29, 6, 0, 0);
    check_and_control_light(fakeNow);

    // Light OFF at noon
    fakeNow = DateTime(2024, 7, 29, 12, 0, 0);
    check_and_control_light(fakeNow);

    // Light ON in evening window
    fakeNow = DateTime(2024, 7, 29, 15, 30, 0);
    check_and_control_light(fakeNow);
}

// --- TEST: Float Switch Safety Cutoff ---
void test_float_switch_safety_cutoff() {
    float waterTemp = 25.0f;

    // Simulate Float Switch Triggered (Low Water Level)
    mockFloatSwitchState = true;

    // Simulate pump running
    mockMillis = 0;
    check_and_control_pump(waterTemp, mockFloatSwitchState);

    // Expected:
    // - Pump should be forced OFF
    // - Refill valve should be turned ON
}

// --- TEST: Sensor Alert Trigger Logic ---
void test_sensor_alert_trigger() {
    RealTimeData testData = {};

    // Case 1: High Turbidity (>800 NTU)
    testData.turbidityNTU = 900.0;
    testData.waterTemp = 25.0;
    testData.pH = 7.0;
    testData.dissolvedOxygen = 5.0;

    DateTime fakeNow(2024, 7, 29, 10, 0, 0);
    apply_rules(testData, fakeNow);

    // Case 2: Low pH (<5.5)
    testData.turbidityNTU = 100.0;
    testData.pH = 5.0;
    apply_rules(testData, fakeNow);

    // Case 3: Low DO (<3.0 mg/L)
    testData.pH = 7.0;
    testData.dissolvedOxygen = 2.5;
    apply_rules(testData, fakeNow);
}

// --- TEST: Comprehensive Schedule Simulation ---
void test_full_schedule_scenario() {
    RealTimeData testData = {};
    testData.airTemp = 29.0f;
    testData.airHumidity = 90.0f;
    testData.waterTemp = 31.0f;
    testData.pH = 7.0f;
    testData.dissolvedOxygen = 5.0f;
    testData.turbidityNTU = 100.0f;
    testData.floatTriggered = false;

    // 5:30 AM - Light should turn ON
    DateTime fakeNow(2024, 7, 29, 5, 30, 0);
    apply_rules(testData, fakeNow);

    // 10:00 AM - Fan turns ON (high temp/humidity)
    fakeNow = DateTime(2024, 7, 29, 10, 0, 0);
    mockMillis = 0;
    apply_rules(testData, fakeNow);

    // Fast forward Fan runtime exceeded
    mockMillis += FAN_MAX_CONTINUOUS_MS + 60000;
    apply_rules(testData, fakeNow);

    // 3:00 PM - Evening lights ON
    fakeNow = DateTime(2024, 7, 29, 15, 0, 0);
    apply_rules(testData, fakeNow);
}

// --- SETUP AND RUN TESTS ---
void setup() {
    delay(2000);
    getMillis = mockMillisFunc;  // Hook mockMillis into rule_engine
    UNITY_BEGIN();
    RUN_TEST(test_fan_runtime_limit);
    RUN_TEST(test_pump_schedule_cycle);
    RUN_TEST(test_light_schedule_logic);
    RUN_TEST(test_full_schedule_scenario);
    RUN_TEST(test_float_switch_safety_cutoff);
    RUN_TEST(test_sensor_alert_trigger);
    UNITY_END();
}


void loop() {
    // Not used
}
