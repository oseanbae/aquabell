#pragma once

#include "sensor_data.h"
extern int currentPage;

void lcd_init();
void lcd_display_update(const RealTimeData& RealTimeData);