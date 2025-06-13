#include <Arduino.h>
#include "config.h"


// Optional: Threshold to detect voltage near ESP32 ADC max input
#define MAX_ADC_SAFE_VOLTAGE 3.1    // Warn if voltage exceeds this

// --- FUNCTION: Read average voltage from the turbidity sensor ---
float readAverageVoltage() {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(TURBIDITY_PIN);
    delay(5);  // Let ADC settle between reads
  }
  float averageRaw = sum / float(NUM_SAMPLES);
  float voltage = averageRaw * (VOLTAGE_REF / ADC_MAX);  // Post-divider voltage

  // Optional: Warn if voltage is too high
  if (voltage > MAX_ADC_SAFE_VOLTAGE) {
    Serial.println("⚠️  WARNING: Voltage approaching or exceeding ADC safe limit!");
  }

  return voltage;
}

// --- FUNCTION: Calculate NTU from voltage ---
// Calibration assumes voltage is already post-divider (i.e., 0–3.0V range)
float calculateNTU(float sensorVoltage) {
  float ntu = 675.0 * sensorVoltage * sensorVoltage - 3120.0 * sensorVoltage + 3650.0;
  return max(ntu, 0.0f);
}

// --- FUNCTION: Classify turbidity into qualitative bands ---
String classifyTurbidity(float ntu) {
  if (ntu <= 70) return "Excellent ";
  if (ntu <= 150) return "Good";
  if (ntu <= 300) return "Moderate ";
  if (ntu <= 500) return "Poor";
  return "Very Poor";
}

// --- FUNCTION: Setup Turbidity Sensor ---
void turbidity_sensor_init() {
  Serial.begin(115200);
  delay(2000); // Optional warm-up for sensor stability
  pinMode(TURBIDITY_PIN, INPUT);
  analogSetAttenuation(ADC_11db);  // Set input range to 0–3.3V

  Serial.println("ESP32 Water Quality Monitor Initialized.");
  Serial.println("Monitoring turbidity based on custom aquaculture standards.");
  Serial.println("-----------------------------------------------------");
}

// --- FUNCTION: Read and print one turbidity sample ---
void turbidity_sensor_read() {
  float dividedVoltage = readAverageVoltage();
  float sensorVoltage = dividedVoltage * 1.5f;
  float turbidity = calculateNTU(sensorVoltage);  // now you're passing real voltage
    // Still pass the divided voltage
  String quality = classifyTurbidity(turbidity);

  Serial.print("ESP32 Voltage (divided): ");
  Serial.print(dividedVoltage, 3);
  Serial.print(" V | Sensor Voltage: ");
  Serial.print(sensorVoltage, 3);
  Serial.print(" V | Turbidity: ");
  Serial.print(turbidity, 1);
  Serial.print(" NTU | Quality: ");
  Serial.println(quality);

  delay(5000);
}
