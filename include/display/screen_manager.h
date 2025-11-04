#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <TFT_eSPI.h>
#include "config/display_config.h"

// --- TYPY EKRANÓW (zdefiniowane w display_config.h) ---

// --- ZARZĄDZANIE EKRANAMI ---
extern ScreenType currentScreen;
extern unsigned long lastScreenSwitch;
extern const unsigned long SCREEN_SWITCH_INTERVAL; // 5 sekund

// --- FUNKCJE ---
void updateScreenManager();
void switchToNextScreen(TFT_eSPI& tft);
void forceScreenRefresh(TFT_eSPI& tft);

#endif