#include <Arduino.h>
#include "config.h"
#include "RTClib.h"
#include "relay_control.h"
#include "sensor_data.h"


unsigned long humidityHighSince = 0; // Timestamp when high humidity condition started
bool humidityAlertActive = false;

void checkHumidityAlert(float humidity, float airTemp, int currentMinutes) {
    const int MONITOR_START = 600;  // 10:00 AM
    const int MONITOR_END   = 930;  // 3:30 PM

    bool inMonitoringWindow = currentMinutes >= MONITOR_START && currentMinutes <= MONITOR_END;
    bool conditionMet = (humidity > 80.0 && airTemp > 30.0);

    if (inMonitoringWindow && conditionMet) {
        if (humidityHighSince == 0) {
            humidityHighSince = millis(); // Start timer
        } else if ((millis() - humidityHighSince) >= 10UL * 60 * 1000) {
            if (!humidityAlertActive) {
                humidityAlertActive = true;
                control_fan(true); // turn fan ON
                Serial.println("⚠️ High humidity+temp for 10+ min — Fan ON");
            }
        }
    } else {
        humidityHighSince = 0;
        if (humidityAlertActive) {
            humidityAlertActive = false;
            control_fan(false);
            Serial.println("✅ Humidity/Temp normal — Fan OFF");
        }
    }
}

void apply_rules(const SensorData& data, const DateTime& now) {
    int currentMinutes = now.hour() * 60 + now.minute();

    checkHumidityAlert(data.airHumidity, data.airTemp, currentMinutes);
    
    if (!isnan(data.airTemp)) {
        control_fan(data.airTemp > TEMP_THRESHOLD);
    }

    if (data.floatTriggered) {
        control_pump(false);
    }

    if (!isnan(data.turbidityNTU) && data.turbidityNTU > 50) {
        control_valve(true);
    } else {
        control_valve(false);
    }

    if (!isnan(data.waterTemp)) {
        control_light(data.waterTemp < 22.0);
    }

    if (!isnan(data.pH) && (data.pH < 5.5 || data.pH > 8.5)) {
        // Add warning LED or alert
    }
}
