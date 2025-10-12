#pragma once
#include "sensor_data.h"
#include <Arduino.h>
#include <time.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h> 

// Command structure for RTDB
struct CommandState {
    bool isAuto;
    bool value;
};

struct Commands {
    CommandState fan;
    CommandState light;
    CommandState pump;
    CommandState valve;
};

void pushToRTDBLive(const RealTimeData &data);
void pushBatchLogToFirestore(RealTimeData *buffer, int size, time_t timestamp);
void firebaseSignIn();
bool fetchControlCommands();
void syncRelayState(const RealTimeData &data, const Commands& commands);
void refreshIdToken();

// New FirebaseClient stream-based functions
void startFirebaseStream();
void handleFirebaseStream();
void onRTDBStream(AsyncResult &result);
bool isStreamConnected();
bool isFirebaseReady();
bool isInitialCommandsSynced();

// Helper function for processing patch events
void processPatchEvent(const String& dataPath, JsonDocument& patchData, Commands& currentCommands, bool& hasChanges);

// Non-blocking retry management
bool processRetry();
bool isRetryInProgress();
