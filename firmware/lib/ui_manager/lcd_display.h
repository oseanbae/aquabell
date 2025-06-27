#pragma once

#include "sensor_data.h"
extern int currentPage;

void lcd_init();
void lcd_display(SensorData data, int currentPage);
void handle_button_press(const SensorData& sensorData);
void lcd_backlight_idle_check();