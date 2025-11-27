#ifndef WIFI_TOUCH_INTERFACE_H
#define WIFI_TOUCH_INTERFACE_H

// WiFi Loss Timeout Configuration
#define WIFI_LOSS_TIMEOUT 5000    // 60 sekund (60,000 ms) - czas po którym przejdzie do scan mode

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>

// --- WiFi STATES ---
enum WiFiState {
  STATE_SCAN_NETWORKS = 0,
  STATE_ENTER_PASSWORD = 1,
  STATE_CONNECTING = 2,
  STATE_CONNECTED = 3,
  STATE_FAILED = 4,
  STATE_CONFIG_MODE = 5
};

// --- COLORS ---
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define PURPLE  0xF81F
#define CYAN    0x07FF

// --- TOUCH CALIBRATION PINS ---
#define TOUCH_CS   21
#define T_IRQ  -1  // Not used

// --- MAIN FUNCTIONS ---
void initWiFiTouchInterface();
void handleWiFiTouchLoop(TFT_eSPI& tft);
bool checkWiFiLongPress(TFT_eSPI& tft);
void enterWiFiConfigMode(TFT_eSPI& tft);
bool isWiFiConfigActive();
void exitWiFiConfigMode();

// --- WIFI FUNCTIONS ---
void scanNetworks();
void connectToWiFi();
void saveCredentials(String ssid, String password);
bool loadCredentials(String& ssid, String& password);

// --- DISPLAY FUNCTIONS ---
void drawNetworkList(TFT_eSPI& tft);
void drawPasswordScreen();
void drawConnectedScreen(TFT_eSPI& tft);
void drawConfigModeScreen();
void drawConnectingScreen();
void drawFailedScreen();
void drawStatusMessage(TFT_eSPI& tft, String message);
void drawKeyboard();
void enterConfigMode();

// --- TOUCH FUNCTIONS ---
void handleTouchInput(int16_t x, int16_t y);
void handleKeyboardTouch(int16_t x, int16_t y);
bool getTouchPoint(int16_t &x, int16_t &y);
uint16_t readTouch(uint8_t command);

// --- BACKGROUND FUNCTIONS ---
void checkWiFiConnection();
void handleWiFiLoss();
void handleBackgroundReconnect();
void handleLongPress(TFT_eSPI& tft);
void handleConfigModeTimeout();

#endif