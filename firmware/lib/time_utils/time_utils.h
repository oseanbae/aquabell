#pragma once
#include <Arduino.h>
#include <time.h>

// === NTP Time Management ===
// Lightweight NTP-based time utility for ESP32
// Philippine Standard Time (UTC+8) with fallback behavior

// Initialize NTP time sync (blocking with timeout)
// Returns true if time was successfully obtained
bool syncTimeOncePerBoot(unsigned long timeoutMs = 20000);

// Get current Unix timestamp (epoch)
// Returns 0 if time is not available
time_t getUnixTime();

// Get local time as tm structure
// Returns true if time is available, false otherwise
bool getLocalTm(struct tm &out);

// Format timestamp as ISO date string (YYYY-MM-DD HH:MM:SS)
String formatDateTime(time_t t);

// Check if time is currently available
bool isTimeAvailable();

// Get current time in minutes since midnight (for scheduling)
// Returns -1 if time is not available
int getCurrentMinutes();

// Periodic re-sync (call from main loop)
// Attempts to re-sync every 24 hours when WiFi is available
void periodicTimeSync();

