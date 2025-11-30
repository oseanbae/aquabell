#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "sensor_data.h"

// 20x4 I2C LCD
#define LCD_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// update interval (ms)
const unsigned long UPDATE_INTERVAL = 1000;
unsigned long lastUpdateMs = 0;

// Helper: pad or truncate a String to exactly width chars (right-padded with spaces)
String fixedWidth(const String &s, uint8_t width) {
    if ((int)s.length() > width) return s.substring(0, width);
    String out = s;
    while ((int)out.length() < width) out += ' ';
    return out;
}

void lcd_init() {
    lcd.init();
    lcd.backlight();
    // print static labels once
    lcd.setCursor(0, 0);
    lcd.print(fixedWidth("Air: ", 5) + fixedWidth("Hum:", 8));
    lcd.setCursor(0, 1);
    lcd.print(fixedWidth("Water: ", 8) + fixedWidth("pH:", 6));
    lcd.setCursor(0, 2);
    lcd.print(fixedWidth("DO:", 8));
    lcd.setCursor(0, 3);
    lcd.print(fixedWidth("Turb:", 8));
}

// Write the dynamic values in-place (no lcd.clear())
void lcd_display(const RealTimeData& data) {

    // --- AIR ---
    String airVal = "Air:" + String(data.airTemp, 1) + "C";   // 1 decimal
    String humVal = "Hum:" + String(data.airHumidity, 0) + "%"; // whole number
    lcd.setCursor(0, 0);
    lcd.print(fixedWidth(airVal, 10));
    lcd.setCursor(10, 0);
    lcd.print(fixedWidth(humVal, 10));

    // --- WATER + pH ---
    String waterVal = "Water: " + String(data.waterTemp, 1) + "C"; // 1 decimal
    String pHVal    = "pH: "    + String(data.pH, 1);              // 1 decimal
    lcd.setCursor(0, 1);
    lcd.print(fixedWidth(waterVal, 12));
    lcd.setCursor(12, 1);
    lcd.print(fixedWidth(pHVal, 8));

    // --- DO ---
    String doVal = "DO: " + String(data.dissolvedOxygen, 1) + "mg/L"; // 1 decimal
    lcd.setCursor(0, 2);
    lcd.print(fixedWidth(doVal, 20));

    // --- TURBIDITY ---
    String turbVal = "Turb: " + String(data.turbidityNTU) + "NTU";
    lcd.setCursor(0, 3);
    lcd.print(fixedWidth(turbVal, 20));
}

