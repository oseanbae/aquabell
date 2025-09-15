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
void syncRelayState(const RealTimeData &data, const Commands& commands);

// Non-blocking retry management
bool processRetry();
bool isRetryInProgress();

// Firestore functions - SENSOR DATA ONLY
// void pushSensorLogs(const RealTimeData& data, time_t timestamp);  // 5-min averaged sensor logs
// void pushActuatorLogsToFirestore(const ActuatorState& actuators, time_t timestamp);  // Optional historical logging

// RTDB functions - COMMANDS ONLY
// Note: We now only use 'commands' in RTDB, no separate 'actuator_states'
// The syncRelayState function handles syncing AUTO actuator values to RTDB
