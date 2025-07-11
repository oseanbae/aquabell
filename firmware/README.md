# 🌿 Aquabell Sensor Node - Smart Aquaponics System

This ESP32-based firmware powers the **Sensor Node** in the Aquabell Smart Aquaponics System. It reads environmental parameters in real-time, transmits data via ESP-NOW, and displays sensor stats on an LCD interface.

---

## 📡 Features

- **Sensors Supported:**
  - 🌡️ Water Temperature (DS18B20)
  - 🧪 pH Level (Analog pH Sensor) – *(calibration pending)*
  - 💨 Dissolved Oxygen (DFRobot SEN0244)
  - 💧 Turbidity (Analog)
  - 🌬️ Air Temp & Humidity (DHT11/DHT22)
  - 🚨 Float Switch (Water level monitoring)

- **Core Functions:**
  - Real-time sensor readings every few seconds
  - Hourly-averaged batch transmission every 5 minutes
  - ESP-NOW transmission to control node (offline-capable)
  - Multi-page LCD interface (I2C, 16x2)
  - Float switch alert with LED + buzzer
  - RAM + LittleFS hybrid buffering (planned)
  - Debounced sensor logic + non-blocking loop design

---

## ⚙️ Setup & Hardware

| Component         | Pin Used           |
|------------------|--------------------|
| DS18B20          | `ONE_WIRE_BUS`     |
| pH Sensor        | `PH_SENSOR_PIN`    |
| DO Sensor        | `DO_SENSOR_PIN`    |
| Turbidity Sensor | `TURBIDITY_PIN`    |
| DHT11/DHT22      | `DHT_PIN`          |
| Float Switch     | `FLOAT_SWITCH_PIN` |
| LCD I2C          | `LCD_ADDR`, SDA/SCL |
| Buzzer           | `BUZZER_PIN`       |
| LED              | `LED_PIN`          |
| Next Button      | `BUTTON_NEXT`      |

> See `config.h` for full pin mapping and read intervals.

---

## 📦 File Structure

```bash
📁 src/
│
├── main.cpp                # Main logic
├── config.h         # Pins, intervals, constants
│
├── sensor_data.h           # RealTimeData + BatchData structs
├── buffer.cpp              # Averaging + reset logic
│
├── tx_espnow.cpp           # ESP-NOW init + send
├── lcd_display.cpp         # LCD display and button logic
│
├── temp_sensor.cpp         # DS18B20
├── ph_sensor.cpp           # Analog pH
├── do_sensor.cpp           # Dissolved Oxygen
├── dht_sensor.cpp          # DHT11/22
├── float_switch.cpp        # Water level logic
├── turbidity_sensor.cpp    # NTU computation
