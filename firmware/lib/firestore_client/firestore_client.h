#pragma once
#include "sensor_data.h"
#include <Arduino.h>
#include <time.h>

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

void pushToFirestoreLive(const RealTimeData &data);
void pushBatchLogToFirestore(RealTimeData *buffer, int size, time_t timestamp);
void firebaseSignIn();
bool fetchControlCommands();
bool fetchCommandsFromRTDB(Commands& commands);
void syncRelayState(const RealTimeData &data);
