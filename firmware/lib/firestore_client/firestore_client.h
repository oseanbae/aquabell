#pragma once
#include "sensor_data.h"
#include <Arduino.h>
#include <time.h>

void pushToFirestoreLive(const RealTimeData &data);
void pushBatchLogToFirestore(RealTimeData *buffer, int size, time_t timestamp);
void firebaseSignIn();
bool fetchControlCommands();
void syncRelayState(const RealTimeData &data);
