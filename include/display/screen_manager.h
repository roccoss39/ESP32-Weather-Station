#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <TFT_eSPI.h>
#include "config/display_config.h"

// --- TYPY EKRANÓW (używamy z ScreenManager.h) ---
// Note: ScreenType enum jest teraz w ScreenManager.h

class ScreenManager;

// Singleton instance ScreenManager
ScreenManager& getScreenManager();

void updateScreenManager();
void switchToNextScreen(TFT_eSPI& tft);
void forceScreenRefresh(TFT_eSPI& tft);

#endif