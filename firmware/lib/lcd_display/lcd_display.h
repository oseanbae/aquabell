#pragma once
#include <Arduino.h>
#include <sensor_data.h>
#include <LiquidCrystal_I2C.h>

void lcd_init();
void lcd_display_update(const RealTimeData& sensorData);
void lcd_display(const RealTimeData& data, int page);