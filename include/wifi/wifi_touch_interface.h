#ifndef WIFI_TOUCH_INTERFACE_H
#define WIFI_TOUCH_INTERFACE_H

// Limity czasu WiFi są teraz w timing_config.h

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config/timing_config.h"

// --- STANY WiFi ---
enum WiFiState {
  STATE_SCAN_NETWORKS = 0,
  STATE_ENTER_PASSWORD = 1,
  STATE_CONNECTING = 2,
  STATE_CONNECTED = 3,
  STATE_FAILED = 4,
  STATE_CONFIG_MODE = 5,
  STATE_SELECT_LOCATION = 6,
  STATE_ENTER_COORDINATES = 7
};

// --- KOLORY ---
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define PURPLE  0xF81F
#define CYAN    0x07FF

// --- PINY KALIBRACJI DOTYKU ---
// TOUCH_CS zdefiniowany w platformio.ini jako flaga kompilacji
#define T_IRQ  -1  // Nieużywane

// --- GŁÓWNE FUNKCJE ---
void initWiFiTouchInterface();
void handleWiFiTouchLoop(TFT_eSPI& tft);
bool checkWiFiLongPress(TFT_eSPI& tft);
void enterWiFiConfigMode(TFT_eSPI& tft);
bool isWiFiConfigActive();
void exitWiFiConfigMode();

// --- FUNKCJE WIFI ---
void scanNetworks();
void connectToWiFi();
void saveCredentials(String ssid, String password);
bool loadCredentials(String& ssid, String& password);

// --- FUNKCJE WYŚWIETLACZA ---
void drawNetworkList(TFT_eSPI& tft);
void drawPasswordScreen();
void drawConnectedScreen(TFT_eSPI& tft);
void drawConfigModeScreen();
void drawConnectingScreen();
void drawFailedScreen();
void drawStatusMessage(TFT_eSPI& tft, String message);
void drawKeyboard();
void enterConfigMode();

// --- FUNKCJE WYBORU LOKALIZACJI ---
void drawLocationScreen(TFT_eSPI& tft);
void handleLocationTouch(int16_t x, int16_t y, TFT_eSPI& tft);
void enterLocationSelectionMode(TFT_eSPI& tft);

// --- FUNKCJE WŁASNYCH WSPÓŁRZĘDNYCH ---
void drawCoordinatesScreen(TFT_eSPI& tft);
void handleCoordinatesTouch(int16_t x, int16_t y, TFT_eSPI& tft);
void enterCoordinatesMode(TFT_eSPI& tft);

// --- FUNKCJE DOTYKU ---
void handleTouchInput(int16_t x, int16_t y);
void handleKeyboardTouch(int16_t x, int16_t y);
bool getTouchPoint(int16_t &x, int16_t &y);
uint16_t readTouch(uint8_t command);

// --- FUNKCJE TŁA ---
void checkWiFiConnection();
void handleWiFiLoss();
void handleBackgroundReconnect();
void handleLongPress(TFT_eSPI& tft);
void handleConfigModeTimeout();

#endif