#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <TFT_eSPI.h>

// --- TYPY EKRANÓW ---
enum ScreenType {
  SCREEN_CURRENT_WEATHER = 0,  // Ekran 1: Aktualna pogoda
  SCREEN_FORECAST = 1          // Ekran 2: Prognoza 3h
};

// --- ZARZĄDZANIE EKRANAMI ---
extern ScreenType currentScreen;
extern unsigned long lastScreenSwitch;
extern const unsigned long SCREEN_SWITCH_INTERVAL; // 5 sekund

// --- FUNKCJE ---
void updateScreenManager();
void switchToNextScreen(TFT_eSPI& tft);
void forceScreenRefresh(TFT_eSPI& tft);

#endif