#include <Arduino.h>
#include "config.h"
#include "RTClib.h"
#include "relay_control.h"
#include "sensor_data.h"

void apply_rules(const SensorData& data) {
    // Example rules

    // 🌀 FAN CONTROL: Turn on if air temp > 30°C
    if (!isnan(data.airTemp)) {
        control_fan(data.airTemp > 30.0);
    }

    // 💧 PUMP: Float switch safety override
    if (data.floatTriggered) {
        control_pump(false); // stop immediately
    }

    // 🌊 TURBIDITY-based warning
    if (!isnan(data.turbidityNTU) && data.turbidityNTU > 50) {
        control_valve(true); // e.g. open clean-water valve
    } else {
        control_valve(false);
    }

    // 🔆 LIGHT (if water temp too low, assume it's cold and lights help)
    if (!isnan(data.waterTemp)) {
        control_light(data.waterTemp < 22.0); // optional logic
    }

    // ⚠️ pH alert action
    if (!isnan(data.pH) && (data.pH < 5.5 || data.pH > 8.5)) {
        // Add logic to blink a warning LED or send alert
    }
}
