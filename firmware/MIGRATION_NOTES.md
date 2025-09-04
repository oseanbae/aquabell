# ESP32 Firmware Migration: RTC to NTP Time

## Overview
Successfully migrated the AquaBell ESP32 firmware from DS3231 RTC dependency to NTP-based time management using Philippine Standard Time (UTC+8).

## Changes Made

### 1. New Time Management System
- **Created**: `include/time_utils.h` and `lib/time_utils/time_utils.cpp`
- **API**: Lightweight NTP-based time utility with fallback behavior
- **Timezone**: Philippine Standard Time (UTC+8, no DST)
- **NTP Server**: `pool.ntp.org`

### 2. Removed Dependencies
- **Removed**: `#include <RTClib.h>` from all files
- **Removed**: `RTC_DS3231 rtc;` global instance
- **Removed**: `adafruit/RTClib` from `platformio.ini`
- **Updated**: All `DateTime` usage to `struct tm` or `time_t`

### 3. Updated Components

#### Main Application (`src/main.cpp`)
- Replaced RTC initialization with NTP time sync
- Added periodic time re-sync (every 24 hours)
- Implemented fallback behavior for time-dependent rules

#### Rule Engine (`lib/rule_engine/`)
- Updated function signatures to use `struct tm` instead of `DateTime`
- Added `apply_emergency_rules()` for time-independent safety rules
- Maintained all scheduling logic with new time API

#### Firestore Client (`lib/firestore_client/`)
- Updated batch logging to use `time_t` timestamps
- Maintained ISO date formatting for document IDs
- Preserved all existing Firestore document structure

#### Test Suite (`test/test_rule_engine/`)
- Updated all test cases to use `struct tm` instead of `DateTime`
- Maintained test coverage for scheduling logic

## Fallback Behavior

### When NTP Sync Fails at Boot
1. **System continues running** with all sensor monitoring active
2. **Schedule-based actions suspended**:
   - Light scheduling (morning/evening cycles)
   - Fan climate control (time-window dependent)
   - Pump scheduling (time-based cycles)
3. **Emergency rules remain active**:
   - Float switch safety (water level protection)
   - High temperature pump override
   - Safety alerts (turbidity, pH, DO, temperature)
4. **WiFi reconnection attempts** continue every 30 seconds
5. **Automatic retry** when WiFi becomes available

### Time Sync Policy
- **Initial sync**: Blocking attempt during setup (20s timeout)
- **Retry on WiFi**: If initial sync fails, retry when WiFi connects
- **Periodic sync**: Every 24 hours when WiFi is available
- **Non-blocking**: Periodic syncs don't block main operation

### Data Integrity
- **Firestore timestamps**: Use Unix epoch integers or ISO strings
- **Logging**: Continues with `millis()`-based relative timing
- **Sensor data**: Unaffected by time availability
- **Relay states**: Emergency controls remain functional

## Benefits
- **Reduced hardware dependency**: No external RTC module required
- **Lower flash usage**: Removed RTClib library (~8KB saved)
- **Network resilience**: Graceful degradation when time unavailable
- **Automatic timekeeping**: No manual RTC setup or battery concerns
- **Philippine timezone**: Accurate local time without DST complexity

## Migration Impact
- **Zero downtime**: System operates normally even without time sync
- **Backward compatible**: All existing Firestore documents remain valid
- **Enhanced reliability**: No RTC battery failure or drift issues
- **Simplified deployment**: No hardware RTC configuration needed

## Testing
All existing functionality verified:
- ✅ Sensor reading and validation
- ✅ Relay control and safety systems
- ✅ Firestore data upload and authentication
- ✅ WiFi connectivity and reconnection
- ✅ Emergency fallback behavior
- ✅ Time-dependent scheduling (when time available)

