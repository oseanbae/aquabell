#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "sensor_data.h"
// ============================
// === 16x2 I2C LCD Display ===
// ============================
#define LCD_SDA               21
#define LCD_SCL               22
#define LCD_ADDR              0x27
#define LCD_COLS              16
#define LCD_ROWS              2
#define LCD_IDLE_TIMEOUT      15000  // ms
#define LCD_BACKLIGHT_ON      true
#define LCD_BACKLIGHT_OFF     false
#define DEBOUNCE_DELAY        50    // ms
#define BUTTON_NEXT           25
#define BUZZER_PIN            26
#define LED_PIN               27
#define TOTAL_PAGES           3

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

// --- CORE DISPLAY FUNCTION ---
void lcd_display(const RealTimeData& data, int page) {
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
            lcd.setCursor(0, 1);
            lcd.print("pH: " + String(data.pH));
            break;
        case 2:
            lcd.setCursor(0, 0);
            lcd.print("DO: " + String(data.dissolvedOxygen) + "mg/L");
            lcd.setCursor(0, 1);
            lcd.print("Turb: " + String(data.turbidityNTU) + "NTU");
            break;
    }
}

// --- DISPLAY UPDATE HANDLER ---
void lcd_display_update(const RealTimeData& sensorData) {
    static unsigned long lastPressTime = 0;
    unsigned long currentTime = millis();

    // --- Handle Button Press ---
    if (digitalRead(BUTTON_NEXT) == LOW && (currentTime - lastPressTime) > DEBOUNCE_DELAY) {
        currentPage = (currentPage + 1) % TOTAL_PAGES;
        lastPressTime = currentTime;
        lastInteractionTime = currentTime;

        if (!backlightOn) {
            lcd.backlight();
            backlightOn = true;
        }

        lcd_display(sensorData, currentPage);  // Show new page

        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);

        return;  // Done for this frame
    }

    // --- Backlight Timeout ---
    if (backlightOn && (currentTime - lastInteractionTime > LCD_IDLE_TIMEOUT)) {
        lcd.noBacklight();
        backlightOn = false;
    }

    // --- Refresh Page if Active ---
    if (backlightOn) {
        lcd_display(sensorData, currentPage);
    }
}

//lcd_display_update(current);