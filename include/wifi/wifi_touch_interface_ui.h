#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// UI layer for WiFi touch interface: drawing + handling touch interactions.
// Logic/state is owned by wifi_touch_interface.cpp.

void wifiTouchUI_drawStatusMessage(TFT_eSPI& tft, const String& message);
void wifiTouchUI_drawConnectedScreen(TFT_eSPI& tft);
void wifiTouchUI_drawNetworkList(TFT_eSPI& tft);
void wifiTouchUI_drawPasswordScreen(TFT_eSPI& tft);
void wifiTouchUI_drawKeyboard(TFT_eSPI& tft);
void wifiTouchUI_drawConfigModeScreen(TFT_eSPI& tft);

void wifiTouchUI_drawLocationScreen(TFT_eSPI& tft);
void wifiTouchUI_drawCoordinatesScreen(TFT_eSPI& tft);

void wifiTouchUI_handleTouchInput(int16_t x, int16_t y, TFT_eSPI& tft);
void wifiTouchUI_handleKeyboardTouch(int16_t x, int16_t y, TFT_eSPI& tft);
void wifiTouchUI_handleLocationTouch(int16_t x, int16_t y, TFT_eSPI& tft);
void wifiTouchUI_handleCoordinatesTouch(int16_t x, int16_t y, TFT_eSPI& tft);

// Visual helpers called from logic loop
void wifiTouchUI_updateConfigModeCountdown(TFT_eSPI& tft, int remainingSeconds);
void wifiTouchUI_drawLongPressProgress(TFT_eSPI& tft, int progressPercent);
