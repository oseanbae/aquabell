#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "sensor_data.h"

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// --- STATE TRACKING ---
int currentPage = 0;
unsigned long lastInteractionTime = 0;
bool backlightOn = true;

// --- INIT ---
void lcd_init() {
    lcd.init();
    lcd.backlight();
    lcd.clear();

    pinMode(BUTTON_NEXT, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    lastInteractionTime = millis();
}

// --- DISPLAY LOGIC ---
void lcd_display(const SensorData& data, int page) {
    lcd.clear();

    switch (page) {
        case 0:
            lcd.setCursor(0, 0);
            lcd.print("Air: " + String(data.airTemp) + "C");
            lcd.setCursor(0, 1);
            lcd.print("Hum: " + String(data.airHumidity) + "%");
            break;
        case 1:
            lcd.setCursor(0, 0);
            lcd.print("Water: " + String(data.waterTemp) + "C");
            lcd.print(" " + String(TEMP_STATUS_LABEL(data.waterTemp)));
            lcd.setCursor(0, 1);
            lcd.print("pH: " + String(data.pH));
            lcd.print(" " + String(PH_STATUS_LABEL(data.pH)));
            break;
        case 2:
            lcd.setCursor(0, 0);
            lcd.print("DO: " + String(data.dissolvedOxygen) + "mg/L");
            lcd.print(" " + String(DO_STATUS_LABEL(data.dissolvedOxygen)));
            lcd.setCursor(0, 1);
            lcd.print("Turb: " + String(data.turbidityNTU) + "NTU");
            lcd.print(" " + String(NTU_STATUS_LABEL(data.turbidityNTU)));
            break;
    }
}

// --- BUTTON HANDLING ---
void handle_button_press(const SensorData& sensorData) {
    static unsigned long lastPressTime = 0;
    unsigned long currentTime = millis();

    if (digitalRead(BUTTON_NEXT) == LOW && (currentTime - lastPressTime) > DEBOUNCE_DELAY) {
        currentPage = (currentPage + 1) % TOTAL_PAGES;
        lcd_display(sensorData, currentPage);
        lastPressTime = currentTime;
        lastInteractionTime = currentTime;

        if (!backlightOn) {
            lcd.backlight();
            backlightOn = true;
        }

        // Feedback
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
    }
}

// --- BACKLIGHT IDLE TIMEOUT ---
void lcd_backlight_idle_check() {
    if (backlightOn && (millis() - lastInteractionTime > LCD_IDLE_TIMEOUT)) {
        lcd.noBacklight();
        backlightOn = false;
    }
}
