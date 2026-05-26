#pragma once

#include <Arduino.h>

// Zachowuje tryb offline po resetach/deep sleep.
// Przechowywane w NVS (Preferences).

bool loadOfflineModePref();
void saveOfflineModePref(bool offline);
