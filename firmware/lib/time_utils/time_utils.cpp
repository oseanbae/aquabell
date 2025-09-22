#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "time_utils.h"

// === Constants ===
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (8 * 3600)  // UTC+8 (Philippine Standard Time)
#define DAYLIGHT_OFFSET_SEC 0      // No DST in Philippines

// === State Variables ===
static bool timeSynced = false;
static unsigned long lastSyncAttempt = 0;
static unsigned long lastSuccessfulSync = 0;
static const unsigned long SYNC_RETRY_INTERVAL = 24 * 60 * 60 * 1000UL; // 24 hours

// === Helper Functions ===
bool waitForTimeSync(unsigned long timeoutMs) {
    unsigned long startTime = millis();
    
    // Wait a bit longer for initial NTP response
    delay(1000);
    
    while (millis() - startTime < timeoutMs) {
        time_t now = time(nullptr);
        if (now > 1000000000UL) { // Check if we have a reasonable timestamp (after year 2001)
            Serial.printf("[Time] Got timestamp: %ld\n", now);
            return true;
        }
        delay(500); // Check every 500ms instead of 100ms
    }
    return false;
}

// === Public API Implementation ===

bool syncTimeOncePerBoot(unsigned long timeoutMs) {
    if (timeSynced) {
        return true; // Already synced
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Time] WiFi not connected, cannot sync time");
        return false;
    }
    
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    Serial.printf("[Time] Syncing time (timeout: %lu ms)...\n", timeoutMs);
    bool success = waitForTimeSync(timeoutMs);
    
    // If first attempt fails, try alternative NTP servers
    if (!success) {
        Serial.println("[Time] Trying alternative servers...");
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "time.google.com");
        success = waitForTimeSync(5000); // Shorter timeout for retry
        
        if (!success) {
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "time.windows.com");
            success = waitForTimeSync(5000);
        }
    }
    
    if (success) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            timeSynced = true;
            lastSuccessfulSync = millis();
            Serial.printf("[Time] ✅ Time synced successfully: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return true;
        }
    }
    
    Serial.println("[Time] ❌ Time sync failed - continuing without accurate time");
    return false;
}

time_t getUnixTime() {
    if (!timeSynced) {
        return 0;
    }
    
    time_t now = time(nullptr);
    return (now > 1000000000UL) ? now : 0; // Return 0 if timestamp seems invalid
}

bool getLocalTm(struct tm &out) {
    if (!timeSynced) {
        return false;
    }
    
    return getLocalTime(&out);
}

String formatDateTime(time_t t) {
    if (t == 0) {
        return "1970-01-01 00:00:00"; // Default fallback
    }
    
    struct tm timeinfo;
    if (localtime_r(&t, &timeinfo)) {
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buffer);
    }
    
    return "1970-01-01 00:00:00"; // Fallback
}

bool isTimeAvailable() {
    return timeSynced && (getUnixTime() > 0);
}

int getCurrentMinutes() {
    struct tm timeinfo;
    if (getLocalTm(timeinfo)) {
        return timeinfo.tm_hour * 60 + timeinfo.tm_min;
    }
    return -1; // Time not available
}

void periodicTimeSync() {
    unsigned long now = millis();
    
    // Only attempt re-sync if:
    // 1. WiFi is connected
    // 2. Enough time has passed since last attempt (24 hours)
    // 3. We haven't synced yet OR it's been 24 hours since last successful sync
    if (WiFi.status() == WL_CONNECTED && 
        (now - lastSyncAttempt >= SYNC_RETRY_INTERVAL) &&
        (!timeSynced || (now - lastSuccessfulSync >= SYNC_RETRY_INTERVAL))) {
        
        lastSyncAttempt = now;
        Serial.println("[Time] Periodic re-sync...");
        
        // Non-blocking sync attempt (shorter timeout for periodic sync)
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        
        // Check if sync succeeded after a short wait
        delay(2000);
        time_t now_time = time(nullptr);
        if (now_time > 1000000000UL) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                timeSynced = true;
                lastSuccessfulSync = now;
                Serial.println("[Time] ✅ Re-sync successful");
            }
        }
    }
}
