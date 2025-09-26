#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "sensor_data.h"

// ============================
// === 20x4 I2C LCD Display ===
// ============================
#define LCD_SDA        21
#define LCD_SCL        22
#define LCD_ADDR       0x27
#define LCD_COLS       20
#define LCD_ROWS       4

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// --- INIT ---
void lcd_init() {
    lcd.init();
    lcd.backlight();
    lcd.clear();
}

// --- CORE DISPLAY FUNCTION FOR 20x4 ---
void lcd_display(const RealTimeData& data) {
    lcd.clear();

    // Row 0: Air temperature and humidity
    lcd.setCursor(0, 0);
    lcd.print("Air: " + String(data.airTemp, 1) + "C  Hum: " + String(data.airHumidity, 1) + "%");

    // Row 1: Water temperature and pH
    lcd.setCursor(0, 1);
    lcd.print("Water: " + String(data.waterTemp, 1) + "C  pH: " + String(data.pH, 2));

    // Row 2: Dissolved oxygen
    lcd.setCursor(0, 2);
    lcd.print("DO: " + String(data.dissolvedOxygen, 1) + "mg/L");

    // Row 3: Turbidity
    lcd.setCursor(0, 3);
    lcd.print("Turb: " + String(data.turbidityNTU, 1) + "NTU");
}