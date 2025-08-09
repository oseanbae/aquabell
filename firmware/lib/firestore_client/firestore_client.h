#pragma once
#include "sensor_data.h"

void pushToFirestoreLive(const RealTimeData &data);
void pushBatchLogToFirestore(RealTimeData *buffer, int size);
void firebaseSignIn();  // <-- ADD THIS LINE
