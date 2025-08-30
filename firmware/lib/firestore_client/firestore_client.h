#pragma once
#include "sensor_data.h"
#include <Arduino.h>

void pushToFirestoreLive(const RealTimeData &data);
void pushBatchLogToFirestore(RealTimeData *buffer, int size, const DateTime& now);
void firebaseSignIn();
bool fetchControlCommands();
void syncRelayState(const RealTimeData &data);
