#pragma once

#include <Arduino.h>

// Persist offline mode across resets/deep sleep.
// Stored in NVS (Preferences).

bool loadOfflineModePref();
void saveOfflineModePref(bool offline);
