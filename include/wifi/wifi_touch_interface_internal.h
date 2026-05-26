#pragma once

// Wewnętrzny nagłówek współdzielony między wifi_touch_interface.cpp (logika) i wifi_touch_interface_ui.cpp (UI).
// Nie jest częścią publicznego API.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include "wifi/wifi_touch_interface.h" // Enum WiFiState + typy publicznego API

// Globalna instancja wyświetlacza (zdefiniowana gdzie indziej, używana przez wrappery UI)
extern TFT_eSPI tft;

// Deklaracje extern współdzielonego stanu (zdefiniowanego w wifi_touch_interface.cpp)
extern Preferences preferences;
extern WiFiState currentState;
extern int selectedNetworkIndex;
extern String enteredPassword;
extern bool capsLock;
extern bool specialMode;
extern bool showPassword;

extern String defaultSSID;
extern String defaultPassword;
extern String currentSSID;
extern String currentPassword;

extern unsigned long touchStartTime;
extern bool touchActive;
extern bool longPressDetected;
extern unsigned long configModeStartTime;

extern unsigned long lastWiFiCheck;
extern unsigned long wifiLostTime;
extern bool wifiWasConnected;
extern bool wifiLostDetected;

// Flagi między modułami (zdefiniowane w main.cpp)
extern bool isLocationSavePending;
extern unsigned long lastReconnectAttempt;
extern bool backgroundReconnectActive;

extern bool reconnectAttemptInProgress;
extern unsigned long reconnectStartTime;

extern int networkCount;
extern String networkNames[20];
extern int networkRSSI[20];
extern bool networkSecure[20];

// Stan wyboru lokalizacji
// Wewnętrzny enum skopiowany z oryginalnego wifi_touch_interface.cpp
enum LocationMenuState {
  MENU_MAIN,
  MENU_DISTRICTS
};

extern LocationMenuState currentMenuState;
extern int currentLocationIndex;
extern int selectedCityIndex;

extern const char* mainMenuOptions[];

extern String customLatitude;
extern String customLongitude;
extern bool editingLatitude;
extern int coordinatesCursorPos;

// Pomocnicze akcesory
bool wifiTouch_isConnected();
