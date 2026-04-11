#include "wifi/wifi_touch_interface_ui.h"
#include "wifi/wifi_touch_interface_internal.h"

#include "display/display_utils.h"
#include "config/timing_config.h"
#include "config/hardware_config.h"
#include "managers/ScreenManager.h"
#include "managers/MotionSensorManager.h"
#include "config/location_config.h"
#include "wifi/offline_mode_pref.h"
#include "weather/weather_data.h"
#include "weather/forecast_data.h"

// Temporary debug for location/custom GPS rendering
#ifndef DEBUG_LOCATION_UI
#define DEBUG_LOCATION_UI 1
#endif


// Colors duplicated from original file (UI concern)
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define DARK_GREEN 0x0340
#define ORANGE 0xFD20
#define DARK_BLUE 0x001F
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define PURPLE  0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x8410
#define DARKGRAY 0x4208
#define COLOR_BACKGROUND 0x0000

extern bool isOfflineMode;
extern void onWiFiConnectedTasks();
extern void forceScreenRefresh(TFT_eSPI& tft);

static void saveTouchCalibrationToNvs(const uint16_t calData[5]) {
  Preferences touchPrefs;
  if (!touchPrefs.begin("touch_cal", false)) {
    Serial.println("❌ [TOUCH CAL] Nie mozna otworzyc NVS namespace 'touch_cal'.");
    return;
  }

  for (int i = 0; i < 5; ++i) {
    const String key = "c" + String(i);
    touchPrefs.putUShort(key.c_str(), calData[i]);
  }
  touchPrefs.putBool("cal_done", true);
  touchPrefs.end();
}

static void runTouchCalibrationFromMenu(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("KALIBRACJA DOTYKU");
  tft.setTextSize(1);
  tft.setCursor(10, 45);
  tft.println("Dotknij krzyzyki w rogach.");
  tft.setCursor(10, 62);
  tft.println("Wykonaj to precyzyjnie.");
  delay(700);

  uint16_t calData[5] = {0, 0, 0, 0, 0};
  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

  uint16_t* activeCal = getTouchCalibration();
  if (activeCal != nullptr) {
    for (int i = 0; i < 5; ++i) activeCal[i] = calData[i];
  }
  tft.setTouch(calData);
  saveTouchCalibrationToNvs(calData);

  Serial.printf("✅ [TOUCH CAL] Zapisano: [%u, %u, %u, %u, %u]\n",
                calData[0], calData[1], calData[2], calData[3], calData[4]);

  tft.fillScreen(BLACK);
  tft.setTextColor(GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 85);
  tft.println("KALIBRACJA OK");
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 120);
  tft.println("Dane zapisane trwale (NVS).");
  tft.setCursor(10, 140);
  tft.println("Nowe ustawienia aktywne od razu.");
  delay(1200);
}

// ===== Public API wrappers (keep old names from wifi_touch_interface.h) =====

void drawStatusMessage(TFT_eSPI& tft, String message) { wifiTouchUI_drawStatusMessage(tft, message); }
void drawConnectedScreen(TFT_eSPI& tft) { wifiTouchUI_drawConnectedScreen(tft); }
void drawNetworkList(TFT_eSPI& tft) { wifiTouchUI_drawNetworkList(tft); }
void drawPasswordScreen() { wifiTouchUI_drawPasswordScreen(tft); }
void drawKeyboard() { wifiTouchUI_drawKeyboard(tft); }
void drawConfigModeScreen() { wifiTouchUI_drawConfigModeScreen(tft); }

void drawLocationScreen(TFT_eSPI& tft) { wifiTouchUI_drawLocationScreen(tft); }
void handleLocationTouch(int16_t x, int16_t y, TFT_eSPI& tft) { wifiTouchUI_handleLocationTouch(x, y, tft); }
void enterLocationSelectionMode(TFT_eSPI& tft); // implemented in logic file

void drawCoordinatesScreen(TFT_eSPI& tft) { wifiTouchUI_drawCoordinatesScreen(tft); }
void handleCoordinatesTouch(int16_t x, int16_t y, TFT_eSPI& tft) { wifiTouchUI_handleCoordinatesTouch(x, y, tft); }
void enterCoordinatesMode(TFT_eSPI& tft); // implemented in logic file

void handleTouchInput(int16_t x, int16_t y) { wifiTouchUI_handleTouchInput(x, y, tft); }
void handleKeyboardTouch(int16_t x, int16_t y) { wifiTouchUI_handleKeyboardTouch(x, y, tft); }

// ===== Drawing functions (moved from wifi_touch_interface.cpp) =====

void wifiTouchUI_drawStatusMessage(TFT_eSPI& tft, const String& message) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(message, tft.width() / 2, tft.height() / 2);
}

void wifiTouchUI_drawConnectedScreen(TFT_eSPI& tft) {
  tft.fillScreen(DARK_BLUE);

  if (wifiLostDetected || WiFi.status() != WL_CONNECTED) {
    tft.setTextColor(ORANGE);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("UTRACONO POLACZENIE WIFI!", 160, 41);
  } else {
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(30, 50);
    tft.println("POLACZONY!");
  }

  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 100);
  tft.println("Network: " + WiFi.SSID());
  tft.setCursor(10, 120);
  if (WiFi.status() == WL_CONNECTED) tft.println("IP: " + WiFi.localIP().toString());
  else tft.println("IP: ---.---.---.---");

  tft.setCursor(10, 140);
  tft.println("Signal: " + String(WiFi.RSSI()) + " dBm");

  if (wifiLostDetected) {
    unsigned long elapsed = millis() - wifiLostTime;
    int remaining = (WIFI_LOSS_TIMEOUT - elapsed) / 1000;
    if (remaining > 0) {
      tft.fillRect(10, 160, 300, 20, RED);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 165);
      tft.printf("Brak WiFi! Polacz za: %d sek (auto. proba: 9s)", remaining);
    }
  }
}

void wifiTouchUI_drawNetworkList(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(1);
  tft.setFreeFont(NULL);

  tft.setCursor(10, 10);
  tft.println("Wybierz siec WiFi:");

  int yPos = 30;
  int maxNetworks = min(networkCount, 7);
  for (int i = 0; i < maxNetworks; i++) {
    if (i == selectedNetworkIndex) tft.fillRect(0, yPos - 2, 240, 30, BLUE);

    tft.setTextColor(WHITE);
    tft.setCursor(10, yPos + 5);
    if (networkSecure[i]) tft.print("[*] ");
    else tft.print("[ ] ");

    String displayName = networkNames[i];
    if (displayName.length() > 25) displayName = displayName.substring(0, 25) + "...";
    tft.print(displayName);

    tft.setCursor(200, yPos + 5);
    tft.print(networkRSSI[i]);

    yPos += 30;
  }

  // Right buttons
  tft.fillRect(240, 80, 75, 30, ORANGE);
  tft.setTextColor(BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("OFFLINE", 240 + 75 / 2, 80 + 30 / 2);

  tft.fillRect(240, 120, 75, 30, BLUE);
  tft.setTextColor(WHITE);
  tft.drawString("ODSWIEZ", 240 + 75 / 2, 120 + 30 / 2);

  tft.setTextDatum(TL_DATUM);
}

void wifiTouchUI_drawKeyboard(TFT_eSPI& tft) {
  // Copied as-is (uses capsLock/specialMode globals)
  String keys[4][12];

  if (specialMode) {
    String specialKeys[4][12] = {
      {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+"},
      {"{", "}", "|", "\\", ":", "\"", "<", ">", "?", "~", "`", "="},
      {"[", "]", ";", "'", ",", ".", "/", "-", "€", "£", "¥", "§"},
      {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "±", "×"}
    };
    for (int i = 0; i < 4; i++) for (int j = 0; j < 12; j++) keys[i][j] = specialKeys[i][j];
  } else {
    String normalKeys[4][12] = {
      {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "#"},
      {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "!", "?"},
      {"a", "s", "d", "f", "g", "h", "j", "k", "l", ".", "_", "-"},
      {"z", "x", "c", "v", "b", "n", "m", ",", ";", ":", "&", "*"}
    };
    for (int i = 0; i < 4; i++) for (int j = 0; j < 12; j++) keys[i][j] = normalKeys[i][j];
  }

  int keyWidth = 25;
  int keyHeight = 28;
  int startY = 85;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 12; col++) {
      if (keys[row][col] != "") {
        int x = col * (keyWidth + 1) + 2;
        int y = row * (keyHeight + 1) + startY;

        tft.fillRect(x, y, keyWidth, keyHeight, DARKGRAY);
        tft.drawRect(x, y, keyWidth, keyHeight, WHITE);

        String keyLabel = keys[row][col];
        if (!specialMode && capsLock && keyLabel.length() == 1 && keyLabel[0] >= 'a' && keyLabel[0] <= 'z') keyLabel.toUpperCase();

        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(x + 8, y + 12);
        tft.print(keyLabel);
      }
    }
  }

  int specialY = startY + 4 * (keyHeight + 1);

  if (specialMode) {
    tft.fillRect(2, specialY, 35, keyHeight, MAGENTA);
    tft.drawRect(2, specialY, 35, keyHeight, WHITE);
    tft.setTextColor(WHITE);
    tft.setCursor(8, specialY + 10);
    tft.print("ABC");
  } else {
    tft.fillRect(2, specialY, 35, keyHeight, capsLock ? BLUE : DARKGRAY);
    tft.drawRect(2, specialY, 35, keyHeight, WHITE);
    tft.setTextColor(WHITE);
    tft.setCursor(10, specialY + 10);
    tft.print("CAPS");
  }

  tft.fillRect(40, specialY, 35, keyHeight, specialMode ? CYAN : DARKGRAY);
  tft.drawRect(40, specialY, 35, keyHeight, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor(48, specialY + 10);
  tft.print("!@#");

  tft.fillRect(80, specialY, 60, keyHeight, DARKGRAY);
  tft.drawRect(80, specialY, 60, keyHeight, WHITE);
  tft.setCursor(100, specialY + 10);
  tft.print("SPACE");

  tft.fillRect(145, specialY, 35, keyHeight, DARKGRAY);
  tft.drawRect(145, specialY, 35, keyHeight, WHITE);
  tft.setCursor(155, specialY + 10);
  tft.print("DEL");

  tft.fillRect(185, specialY, 50, keyHeight, GREEN);
  tft.drawRect(185, specialY, 50, keyHeight, WHITE);
  tft.setCursor(195, specialY + 10);
  tft.print("CONN");

  tft.fillRect(240, specialY, 75, keyHeight, RED);
  tft.drawRect(240, specialY, 75, keyHeight, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor(255, specialY + 10);
  tft.print("COFNIJ");
}

void wifiTouchUI_drawPasswordScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Wpisz haslo dla:");
  tft.setCursor(10, 25);
  tft.println(networkNames[selectedNetworkIndex]);

  tft.drawRect(10, 50, 220, 25, WHITE);
  tft.fillRect(11, 51, 218, 23, BLACK);
  tft.setCursor(15, 58);

  String displayPassword = "";
  if (showPassword) displayPassword = enteredPassword;
  else for (int i = 0; i < enteredPassword.length(); i++) displayPassword += "*";
  tft.print(displayPassword);

  tft.fillRect(240, 50, 75, 25, showPassword ? GREEN : GRAY);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(250, 58);
  tft.print(showPassword ? "UKRYJ" : "POKAZ");

  wifiTouchUI_drawKeyboard(tft);
}

void wifiTouchUI_drawConfigModeScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("TRYB KONFIGURACJI");

  tft.fillRect(220, 5, 95, 24, DARK_GREEN);
  tft.drawRect(220, 5, 95, 24, WHITE);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("KALIBRACJA", 220 + 95 / 2, 5 + 12);

  int yPos = 35;
  int maxNetworks = min(networkCount, 5);

  for (int i = 0; i < maxNetworks; i++) {
    tft.setTextColor(WHITE);
    tft.setCursor(10, yPos + 5);

    if (networkSecure[i]) tft.print("[*] ");
    else tft.print("[ ] ");

    String displayName = networkNames[i];
    if (displayName.length() > 25) displayName = displayName.substring(0, 25) + "...";
    tft.print(displayName);

    tft.setCursor(270, yPos + 5);
    tft.print(networkRSSI[i]);

    yPos += 25;
  }

  int btnY = 190;
  int btnH = 45;
  int btnW = 75;
  int gap = 3;

  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);

  tft.fillRect(2, btnY, btnW, btnH, BLUE);
  tft.setTextColor(WHITE);
  tft.drawString("ODSWIEZ", 2 + btnW / 2, btnY + btnH / 2);

  tft.fillRect(2 + btnW + gap, btnY, btnW, btnH, MAGENTA);
  tft.setTextColor(WHITE);
  tft.drawString("MIASTO", 2 + btnW + gap + btnW / 2, btnY + btnH / 2);

  if (isOfflineMode) {
    tft.fillRect(2 + 2 * (btnW + gap), btnY, btnW, btnH, GREEN);
    tft.setTextColor(BLACK);
    tft.drawString("ONLINE", 2 + 2 * (btnW + gap) + btnW / 2, btnY + btnH / 2);
  } else {
    tft.fillRect(2 + 2 * (btnW + gap), btnY, btnW, btnH, ORANGE);
    tft.setTextColor(BLACK);
    tft.drawString("OFFLINE", 2 + 2 * (btnW + gap) + btnW / 2, btnY + btnH / 2);
  }

  tft.fillRect(2 + 3 * (btnW + gap), btnY, btnW, btnH, DARKGRAY);
  tft.setTextColor(WHITE);
  tft.drawString("WYJSCIE", 2 + 3 * (btnW + gap) + btnW / 2, btnY + btnH / 2);

  tft.setTextDatum(TL_DATUM);
}

// ===== Location screen (moved from wifi_touch_interface_logic.cpp) =====

void wifiTouchUI_drawLocationScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 5);

  if (currentMenuState == MENU_MAIN) {
    tft.println("WYBIERZ MIASTO");
  } else {
    tft.printf("DZIELNICE: %s", mainMenuOptions[selectedCityIndex]);
  }

  // Określ źródło danych
  const WeatherLocation* cityList = nullptr;
  int cityCount = 0;

  if (currentMenuState == MENU_MAIN) {
    cityCount = 4; // Szczecin, Poznan, Zlocieniec, Wlasny GPS
  } else if (currentMenuState == MENU_DISTRICTS) {
    if (selectedCityIndex == 0) {
      cityList = SZCZECIN_DISTRICTS;
      cityCount = SZCZECIN_DISTRICTS_COUNT;
    } else if (selectedCityIndex == 1) {
      cityList = POZNAN_DISTRICTS;
      cityCount = POZNAN_DISTRICTS_COUNT;
    } else if (selectedCityIndex == 2) {
      cityList = ZLOCIENIEC_AREAS;
      cityCount = ZLOCIENIEC_AREAS_COUNT;
    }
  }

  WeatherLocation currentLoc = locationManager.getCurrentLocation();

#if DEBUG_LOCATION_UI
  Serial.println("[LOC_UI] drawLocationScreen()");
  const char* dn = currentLoc.displayName.c_str();
  Serial.printf("[LOC_UI] currentLoc.displayName len=%d\n", currentLoc.displayName.length());
  Serial.printf("[LOC_UI] currentLoc.displayName='%s'\n", dn);
  // dump first 16 bytes
  Serial.print("[LOC_UI] bytes: ");
  for (int i = 0; i < 16; i++) {
    unsigned char c = (unsigned char)dn[i];
    Serial.printf("%02X ", c);
    if (c == 0) break;
  }
  Serial.println();
#endif

  // Clear a larger area: punctuation like ',' has pixels below the baseline
  // and can remain if the cleared rectangle is too small.
  tft.fillRect(0, 28, tft.width(), 24, BLACK);
#if DEBUG_LOCATION_UI
  // visualize cleared area (optional)
  tft.drawRect(0, 28, tft.width(), 24, DARKGRAY);
#endif
  tft.setTextColor(YELLOW, BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.printf("Aktualna: %s", currentLoc.displayName.c_str());

  tft.setTextColor(WHITE);
  int yPos = 60;
  const int maxVisibleItems = 5;

  int scrollOffset = 0;
  if (currentLocationIndex >= maxVisibleItems) {
    scrollOffset = currentLocationIndex - maxVisibleItems + 1;
  }

  for (int i = 0; i < min(cityCount, maxVisibleItems); i++) {
    int itemIndex = i + scrollOffset;
    if (itemIndex >= cityCount) break;

    bool isSelected = (itemIndex == currentLocationIndex);
    uint16_t bgColor = isSelected ? BLUE : BLACK;

    tft.fillRect(10, yPos - 2, 300, 22, bgColor);
    tft.setTextColor(WHITE);
    tft.setCursor(15, yPos + 2);

    if (currentMenuState == MENU_MAIN) {
      if (isSelected) tft.print("→ ");
      else tft.print("  ");
      tft.printf("%s", mainMenuOptions[itemIndex]);
    } else if (currentMenuState == MENU_DISTRICTS) {
      bool isCurrent = (String(cityList[itemIndex].displayName) == String(currentLoc.displayName));

      if (isCurrent) {
        tft.fillRect(10, yPos - 2, 300, 22, GREEN);
        tft.print("● ");
      } else if (isSelected) {
        tft.print("→ ");
      } else {
        tft.print("  ");
      }

      tft.printf("%s", cityList[itemIndex].displayName);
      tft.setCursor(200, yPos + 2);
      tft.printf("%.4f,%.4f", cityList[itemIndex].latitude, cityList[itemIndex].longitude);
    }

    yPos += 25;
  }

  // Navigation buttons
  tft.fillRect(10, 210, 50, 25, DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(25, 218);
  tft.print("UP");

  tft.fillRect(70, 210, 50, 25, DARKGRAY);
  tft.setCursor(80, 218);
  tft.print("DOWN");

  // Action buttons
  tft.fillRect(130, 210, 60, 25, GREEN);
  tft.setCursor(145, 218);
  tft.print("WYBIERZ");

  tft.fillRect(200, 210, 50, 25, RED);
  tft.setCursor(213, 218);
  tft.print("POWROT");

  tft.fillRect(260, 210, 55, 25, ORANGE);
  tft.setCursor(270, 218);
  tft.print("ZAPISZ");

  if (cityCount > 5) {
    tft.setTextColor(DARKGRAY);
    tft.setTextSize(1);
    tft.setCursor(10, 185);
    if (currentMenuState == MENU_MAIN) tft.printf("(4 opcje - uzyj UP/DOWN)");
    else tft.printf("(%d dzielnic - uzyj UP/DOWN)", cityCount);
  }

  // Custom coordinates button
  tft.fillRect(10, 240, 100, 25, PURPLE);
  tft.setCursor(25, 248);
  tft.print("CUSTOM GPS");
}

void wifiTouchUI_handleLocationTouch(int16_t x, int16_t y, TFT_eSPI& tft) {
  Serial.printf("Location Touch: X=%d, Y=%d\n", x, y);

  // Click list
  if (y >= 60 && y <= 185) {
    int clickedIndex = (y - 60) / 25;

    int scrollOffset = 0;
    if (currentLocationIndex >= 5) scrollOffset = currentLocationIndex - 5 + 1;

    int realIndex = clickedIndex + scrollOffset;

    if (currentMenuState == MENU_MAIN) {
      if (realIndex >= 0 && realIndex < 4) {
        currentLocationIndex = realIndex;
        selectedCityIndex = realIndex;

        if (realIndex == 3) {
          enterCoordinatesMode(tft);
          return;
        }

        currentMenuState = MENU_DISTRICTS;
        currentLocationIndex = 0;
        Serial.printf("Wybrano miasto: %s\n", mainMenuOptions[selectedCityIndex]);
        wifiTouchUI_drawLocationScreen(tft);
        return;
      }
    } else if (currentMenuState == MENU_DISTRICTS) {
      int maxItems = 0;
      if (selectedCityIndex == 0) maxItems = SZCZECIN_DISTRICTS_COUNT;
      else if (selectedCityIndex == 1) maxItems = POZNAN_DISTRICTS_COUNT;
      else if (selectedCityIndex == 2) maxItems = ZLOCIENIEC_AREAS_COUNT;

      if (realIndex >= 0 && realIndex < maxItems) {
        currentLocationIndex = realIndex;
        wifiTouchUI_drawLocationScreen(tft);
        return;
      }
    }
  }

  // Nav buttons
  if (y >= 210 && y <= 235) {
    if (x >= 10 && x <= 60) {
      if (currentLocationIndex > 0) {
        currentLocationIndex--;
        wifiTouchUI_drawLocationScreen(tft);
      }
      return;
    }

    if (x >= 70 && x <= 120) {
      int maxItems = 0;
      if (currentMenuState == MENU_MAIN) maxItems = 4;
      else if (currentMenuState == MENU_DISTRICTS) {
        if (selectedCityIndex == 0) maxItems = SZCZECIN_DISTRICTS_COUNT;
        else if (selectedCityIndex == 1) maxItems = POZNAN_DISTRICTS_COUNT;
        else if (selectedCityIndex == 2) maxItems = ZLOCIENIEC_AREAS_COUNT;
      }

      if (currentLocationIndex < maxItems - 1) {
        currentLocationIndex++;
        wifiTouchUI_drawLocationScreen(tft);
      }
      return;
    }

    if (x >= 130 && x <= 190) {
      if (currentMenuState == MENU_MAIN) {
        if (currentLocationIndex == 3) {
          enterCoordinatesMode(tft);
          return;
        }

        selectedCityIndex = currentLocationIndex;
        currentMenuState = MENU_DISTRICTS;
        currentLocationIndex = 0;
        wifiTouchUI_drawLocationScreen(tft);
        return;
      }

      if (currentMenuState == MENU_DISTRICTS) {
        const WeatherLocation* cityList = nullptr;
        if (selectedCityIndex == 0) cityList = SZCZECIN_DISTRICTS;
        else if (selectedCityIndex == 1) cityList = POZNAN_DISTRICTS;
        else if (selectedCityIndex == 2) cityList = ZLOCIENIEC_AREAS;

        if (cityList) {
          WeatherLocation selectedLocation = cityList[currentLocationIndex];
          locationManager.setLocation(selectedLocation);
          isLocationSavePending = true;
          Serial.printf("Location set to: %s\n", selectedLocation.displayName);

          // reset cached weather
          weather.isValid = false;
          weeklyForecast.isValid = false;
          forecast.isValid = false;

          tft.fillRect(130, 210, 60, 25, YELLOW);
          tft.setTextColor(BLACK);
          tft.setCursor(145, 218);
          tft.print("SET!");
          delay(150);

          extern unsigned long lastWeatherCheckGlobal;
          extern unsigned long lastForecastCheckGlobal;
          extern unsigned long lastWeeklyUpdate;
          extern bool weatherErrorModeGlobal;
          extern bool forecastErrorModeGlobal;
          extern bool weeklyErrorModeGlobal;

          weatherErrorModeGlobal = true;
          forecastErrorModeGlobal = true;
          weeklyErrorModeGlobal = true;
          lastWeatherCheckGlobal = millis() - 20000;
          lastForecastCheckGlobal = millis() - 20000;
          lastWeeklyUpdate = millis() - 15000000;

          // Show loading state for up to 10 seconds after location change
          extern bool isWeatherRefreshInProgress;
          extern unsigned long weatherRefreshStartMs;
          isWeatherRefreshInProgress = true;
          weatherRefreshStartMs = millis();
          extern unsigned long weatherRefreshTimeoutMs;
          weatherRefreshTimeoutMs = 10000UL;

          wifiTouchUI_drawLocationScreen(tft);
        }
        return;
      }
    }

    if (x >= 200 && x <= 250) {
      if (currentMenuState == MENU_DISTRICTS) {
        currentMenuState = MENU_MAIN;
        currentLocationIndex = selectedCityIndex;
        wifiTouchUI_drawLocationScreen(tft);
      } else {
        currentState = STATE_CONFIG_MODE;
        drawConfigModeScreen();
      }
      return;
    }

    if (x >= 260 && x <= 315) {
      tft.fillRect(260, 210, 55, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setCursor(270, 218);
      tft.print("ZAPISANO");
      delay(100);
      currentState = STATE_CONFIG_MODE;
      drawConfigModeScreen();
      return;
    }
  }

  // Custom GPS
  if (y >= 240 && y <= 265 && x >= 10 && x <= 110) {
    tft.fillRect(10, 240, 100, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(25, 248);
    tft.print("OPENING");
    delay(100);
    enterCoordinatesMode(tft);
  }
}

// ===== Coordinates screen (moved from wifi_touch_interface_logic.cpp) =====

void wifiTouchUI_drawCoordinatesScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("WLASNE WSPOLRZEDNE");

  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.println("Wpisz wspolrzedne GPS (dok. 4 miejsca po przecinku):");

  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  tft.setTextColor(GRAY);
  tft.setCursor(10, 50);
  tft.printf("Current: %.4f, %.4f", currentLoc.latitude, currentLoc.longitude);

  uint16_t latColor = editingLatitude ? CYAN : WHITE;
  tft.setTextColor(latColor);
  tft.setTextSize(1);
  tft.setCursor(10, 80);
  tft.print("Szer. geogr. (N/S): ");

  tft.fillRect(130, 75, 100, 20, editingLatitude ? BLUE : DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(135, 80);
  tft.print(customLatitude);

  if (editingLatitude && (millis() / 500) % 2) {
    int cursorX = 135 + coordinatesCursorPos * 6;
    tft.drawLine(cursorX, 95, cursorX + 5, 95, WHITE);
  }

  uint16_t lonColor = !editingLatitude ? CYAN : WHITE;
  tft.setTextColor(lonColor);
  tft.setCursor(10, 110);
  tft.print("Dlug. geogr. (E/W): ");

  tft.fillRect(130, 105, 100, 20, !editingLatitude ? BLUE : DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(135, 110);
  tft.print(customLongitude);

  if (!editingLatitude && (millis() / 500) % 2) {
    int cursorX = 135 + coordinatesCursorPos * 6;
    tft.drawLine(cursorX, 125, cursorX + 5, 125, WHITE);
  }

  tft.fillRect(250, 80, 60, 45, PURPLE);
  tft.setTextColor(WHITE);
  tft.setCursor(255, 95);
  tft.print(editingLatitude ? "DO DLG" : "DO SZR");

  // numeric keyboard (as in original)
  int keyW = 28;
  int keyH = 25;
  int gap = 4;
  int startX = 5;

  int row1[3] = {1, 2, 3};
  for (int i = 0; i < 3; i++) {
    int x = startX + i * (keyW + gap);
    int y = 140;
    tft.fillRect(x, y, keyW, keyH, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(row1[i]);
  }

  int row2[4] = {4, 5, 6, 0};
  for (int i = 0; i < 4; i++) {
    int x = startX + i * (keyW + gap);
    int y = 170;
    tft.fillRect(x, y, keyW, keyH, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(row2[i]);
  }

  int row3[3] = {7, 8, 9};
  for (int i = 0; i < 3; i++) {
    int x = startX + i * (keyW + gap);
    int y = 200;
    tft.fillRect(x, y, keyW, keyH, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(row3[i]);
  }

  int dotX = startX + 3 * (keyW + gap);
  tft.fillRect(dotX, 200, keyW, keyH, DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(dotX + 10, 208);
  tft.print(".");

  int minusX = startX + 4 * (keyW + gap);
  tft.fillRect(minusX, 200, keyW, keyH, DARKGRAY);
  tft.setCursor(minusX + 10, 208);
  tft.print("-");

  int funcX = 175;
  int funcW = 65;
  int backX = 245;
  int backW = 70;

  tft.fillRect(backX, 140, backW, 25, RED);
  tft.setTextColor(WHITE);
  tft.setCursor(backX + 10, 148);
  tft.print("WYCZYSC");

  tft.fillRect(funcX, 170, funcW, 25, GRAY);
  tft.setCursor(funcX + 10, 178);
  tft.print("WYJSCIE");

  tft.fillRect(backX, 170, backW, 25, ORANGE);
  tft.setCursor(backX + 15, 178);
  tft.print("COFNIJ");

  tft.fillRect(funcX, 200, funcW, 25, YELLOW);
  tft.setTextColor(BLACK);
  tft.setCursor(funcX + 20, 208);
  tft.print("TEST");

  tft.fillRect(backX, 200, backW, 25, GREEN);
  tft.setTextColor(WHITE);
  tft.setCursor(backX + 20, 208);
  tft.print("USTAW");

  tft.setTextColor(GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 230);
  tft.print("Format np. 53.4242");
}

void wifiTouchUI_handleCoordinatesTouch(int16_t x, int16_t y, TFT_eSPI& tft) {
  Serial.printf("🎯 Coordinates Touch: X=%d, Y=%d\n", x, y);

  if (y >= 75 && y <= 95 && x >= 130 && x <= 230) {
    editingLatitude = true;
    coordinatesCursorPos = customLatitude.length();
    wifiTouchUI_drawCoordinatesScreen(tft);
    return;
  }

  if (y >= 105 && y <= 125 && x >= 130 && x <= 230) {
    editingLatitude = false;
    coordinatesCursorPos = customLongitude.length();
    wifiTouchUI_drawCoordinatesScreen(tft);
    return;
  }

  if (y >= 80 && y <= 125 && x >= 250 && x <= 310) {
    editingLatitude = !editingLatitude;
    String& currentField = editingLatitude ? customLatitude : customLongitude;
    coordinatesCursorPos = currentField.length();
    wifiTouchUI_drawCoordinatesScreen(tft);
    return;
  }

  String& currentField = editingLatitude ? customLatitude : customLongitude;

  int keyW = 28;
  int gap = 4;
  int startX = 5;

  auto getCol = [&](int tx) -> int {
    if (tx < startX) return -1;
    return (tx - startX) / (keyW + gap);
  };

  if (y >= 140 && y <= 165) {
    int col = getCol(x);
    if (col >= 0 && col <= 2 && x <= startX + col * (keyW + gap) + keyW) {
      currentField += String(col + 1);
      coordinatesCursorPos = currentField.length();
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }
    if (x >= 245 && x <= 315) {
      currentField = "";
      coordinatesCursorPos = 0;
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }
  } else if (y >= 170 && y <= 195) {
    int col = getCol(x);
    if (col >= 0 && col <= 3 && x <= startX + col * (keyW + gap) + keyW) {
      if (col == 3) currentField += "0";
      else currentField += String(col + 4);
      coordinatesCursorPos = currentField.length();
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }
    if (x >= 175 && x <= 240) {
      Serial.println("🚪 EXIT button pressed");
      tft.fillRect(175, 170, 65, 25, YELLOW);
      delay(200);
      currentState = STATE_SELECT_LOCATION;
      wifiTouchUI_drawLocationScreen(tft);
      return;
    }
    if (x >= 245 && x <= 315) {
      Serial.println("↩ BACK button pressed");
      tft.fillRect(245, 170, 70, 25, YELLOW);
      delay(200);
      currentState = STATE_SELECT_LOCATION;
      wifiTouchUI_drawLocationScreen(tft);
      return;
    }
  } else if (y >= 200 && y <= 225) {
    int col = getCol(x);
    if (col >= 0 && col <= 2 && x <= startX + col * (keyW + gap) + keyW) {
      currentField += String(col + 7);
      coordinatesCursorPos = currentField.length();
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }
    if (x >= startX + 3 * (keyW + gap) && x <= startX + 3 * (keyW + gap) + keyW) {
      currentField += ".";
      coordinatesCursorPos = currentField.length();
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }
    if (x >= startX + 4 * (keyW + gap) && x <= startX + 4 * (keyW + gap) + keyW) {
      currentField += "-";
      coordinatesCursorPos = currentField.length();
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }

    if (x >= 175 && x <= 240) {
      Serial.println("🧪 TEST button pressed");
      tft.fillRect(175, 200, 65, 25, YELLOW);
      delay(200);
      wifiTouchUI_drawCoordinatesScreen(tft);
      return;
    }
    if (x >= 245 && x <= 315) {
      Serial.println("✅ SET button pressed");
      tft.fillRect(245, 200, 70, 25, YELLOW);
      delay(200);

      double lat = customLatitude.toDouble();
      double lon = customLongitude.toDouble();

      WeatherLocation customLoc;
      // Format with hemispheres (N/S, E/W)
      const char latHem = (lat >= 0.0) ? 'N' : 'S';
      const char lonHem = (lon >= 0.0) ? 'E' : 'W';
      const double latAbs = fabs(lat);
      const double lonAbs = fabs(lon);
      customLoc.displayName = String("GPS ") + String(latAbs, 4) + latHem + ", " + String(lonAbs, 4) + lonHem;
      customLoc.latitude = lat;
      customLoc.longitude = lon;

#if DEBUG_LOCATION_UI
      Serial.println("[LOC_UI] SET custom GPS");
      Serial.printf("[LOC_UI] lat=%.6f lon=%.6f\n", lat, lon);
      Serial.printf("[LOC_UI] displayName='%s'\n", customLoc.displayName.c_str());
#endif

      locationManager.setLocation(customLoc);
      isLocationSavePending = true;

      // reset cached weather (same behavior as district selection)
      weather.isValid = false;
      weeklyForecast.isValid = false;
      forecast.isValid = false;

      extern unsigned long lastWeatherCheckGlobal;
      extern unsigned long lastForecastCheckGlobal;
      extern unsigned long lastWeeklyUpdate;
      extern bool weatherErrorModeGlobal;
      extern bool forecastErrorModeGlobal;
      extern bool weeklyErrorModeGlobal;

      weatherErrorModeGlobal = true;
      forecastErrorModeGlobal = true;
      weeklyErrorModeGlobal = true;
      lastWeatherCheckGlobal = millis() - 20000;
      lastForecastCheckGlobal = millis() - 20000;
      lastWeeklyUpdate = millis() - 15000000;

      // Show loading state for up to 10 seconds after location change
      extern bool isWeatherRefreshInProgress;
      extern unsigned long weatherRefreshStartMs;
      isWeatherRefreshInProgress = true;
      weatherRefreshStartMs = millis();
      extern unsigned long weatherRefreshTimeoutMs;
      weatherRefreshTimeoutMs = 10000UL;

      currentState = STATE_SELECT_LOCATION;
      wifiTouchUI_drawLocationScreen(tft);
      return;
    }
  }
}

void wifiTouchUI_updateConfigModeCountdown(TFT_eSPI& tft, int remainingSeconds) {
  tft.fillRect(250, 10, 65, 20, BLACK);
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(250, 15);
  tft.printf("Exit: %d", remainingSeconds);
}

void wifiTouchUI_drawLongPressProgress(TFT_eSPI& tft, int progressPercent) {
  tft.fillRect(10, 10, 300, 20, BLACK);
  tft.drawRect(10, 10, 300, 20, WHITE);
  if (progressPercent > 0) tft.fillRect(12, 12, (progressPercent * 296) / 100, 16, YELLOW);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Trzymaj...", 130, 35);
}

// ===== Touch handlers (still UI/controller). The full original implementations will be moved next step. =====

void wifiTouchUI_handleKeyboardTouch(int16_t x, int16_t y, TFT_eSPI& tft) {
  (void)tft;
  int keyWidth = 25;
  int keyHeight = 28;
  int startY = 85;

  // SHOW/HIDE password
  if (y >= 50 && y <= 75 && x >= 240 && x <= 315) {
    showPassword = !showPassword;
    Serial.printf("Password visibility toggled: %s\n", showPassword ? "POKAZ" : "UKRYJ");

    tft.fillRect(240, 50, 75, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(255, 58);
    tft.print("TOGGLE");
    delay(200);

    drawPasswordScreen();
    return;
  }

  // Main keyboard grid
  if (y >= startY && y <= startY + 4 * (keyHeight + 1)) {
    int row = (y - startY) / (keyHeight + 1);
    int col = (x - 2) / (keyWidth + 1);

    if (row >= 0 && row < 4 && col >= 0 && col < 12) {
      String keys[4][12];

      if (specialMode) {
        String specialKeys[4][12] = {
          {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+"},
          {"{", "}", "|", "\\", ":", "\"", "<", ">", "?", "~", "`", "="},
          {"[", "]", ";", "'", ",", ".", "/", "-", "€", "£", "¥", "§"},
          {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "±", "×"}
        };
        for (int i = 0; i < 4; i++) for (int j = 0; j < 12; j++) keys[i][j] = specialKeys[i][j];
      } else {
        String normalKeys[4][12] = {
          {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "#"},
          {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "!", "?"},
          {"a", "s", "d", "f", "g", "h", "j", "k", "l", ".", "_", "-"},
          {"z", "x", "c", "v", "b", "n", "m", ",", ";", ":", "&", "*"}
        };
        for (int i = 0; i < 4; i++) for (int j = 0; j < 12; j++) keys[i][j] = normalKeys[i][j];
      }

      String key = keys[row][col];
      if (key != "") {
        if (!specialMode && capsLock && key.length() == 1 && key[0] >= 'a' && key[0] <= 'z') {
          key.toUpperCase();
        }
        enteredPassword += key;
        Serial.printf("Added character: %s\n", key.c_str());
        drawPasswordScreen();
      }
    }
    return;
  }

  // Special row
  if (y >= startY + 4 * (keyHeight + 1) && y <= startY + 5 * (keyHeight + 1)) {
    if (x >= 2 && x <= 37) {
      if (specialMode) {
        specialMode = false;
        Serial.println("Switched to ABC mode");
      } else {
        capsLock = !capsLock;
        Serial.println("CAPS LOCK toggled");
      }
      drawPasswordScreen();
      return;
    }

    if (x >= 40 && x <= 75) {
      specialMode = !specialMode;
      Serial.printf("Special mode: %s\n", specialMode ? "ON" : "OFF");
      drawPasswordScreen();
      return;
    }

    if (x >= 80 && x <= 140) {
      enteredPassword += " ";
      Serial.println("SPACE added");
      drawPasswordScreen();
      return;
    }

    if (x >= 145 && x <= 180) {
      if (enteredPassword.length() > 0) {
        enteredPassword.remove(enteredPassword.length() - 1);
        Serial.println("CHARACTER deleted");
        drawPasswordScreen();
      }
      return;
    }

    if (x >= 185 && x <= 235) {
      Serial.println("CONNECT button pressed");
      connectToWiFi();
      return;
    }

    if (x >= 240 && x <= 315) {
      Serial.println("BACK to network list");
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList(tft);
      return;
    }
  }
}

void wifiTouchUI_handleTouchInput(int16_t x, int16_t y, TFT_eSPI& tft) {
  Serial.printf("HandleTouch: State=%d, X=%d, Y=%d\n", currentState, x, y);

  if (currentState == STATE_SCAN_NETWORKS) {
    tft.fillCircle(x, y, 5, YELLOW);
    delay(100);

    if (x >= 240 && x <= 315 && y >= 80 && y <= 110) {
      Serial.println("BTN: OFFLINE MODE (Scan Screen)");

      tft.fillRect(240, 80, 75, 30, YELLOW);
      tft.setTextColor(BLACK);
      tft.setCursor(250, 90);
      tft.println("OK!");
      delay(500);

      isOfflineMode = true;
      saveOfflineModePref(true);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);

      currentState = STATE_CONNECTED;
      tft.fillScreen(BLACK);
      wifiLostDetected = false;

      clearAndShowMessage(tft, "Podlacz jednorazowo WiFi do synchro. czasu");
      delay(2000);

      extern ScreenManager& getScreenManager();
      getScreenManager().setCurrentScreen(SCREEN_LOCAL_SENSORS);
      getScreenManager().resetScreenTimer();
      forceScreenRefresh(tft);
      return;
    }

    if (x >= 240 && x <= 315 && y >= 120 && y <= 150) {
      Serial.println("REFRESH BUTTON PRESSED");

      tft.fillRect(240, 120, 75, 30, GREEN);
      tft.setTextColor(WHITE);
      tft.setTextSize(1);
      tft.setCursor(250, 130);
      tft.println("SCANNING");
      delay(500);

      selectedNetworkIndex = -1;
      scanNetworks();

      if (networkCount > 0) {
        drawNetworkList(tft);
      } else {
        drawStatusMessage(tft, "Brak sieci - Sprobuj ponownie");
        delay(2000);
        scanNetworks();
        drawNetworkList(tft);
      }
      return;
    }

    if (x < 240 && y >= 25 && y <= 240) {
      int selectedIndex = (y - 30) / 30;

      if (selectedIndex >= 0 && selectedIndex < min(networkCount, 7)) {
        selectedNetworkIndex = selectedIndex;
        currentSSID = networkNames[selectedIndex];
        Serial.printf("NETWORK SELECTED: %s (index %d)\n", currentSSID.c_str(), selectedIndex);

        tft.fillRect(0, 30 + selectedIndex * 30 - 2, 240, 30, GREEN);
        delay(300);

        if (networkSecure[selectedIndex]) {
          currentState = STATE_ENTER_PASSWORD;
          enteredPassword = "";
          drawPasswordScreen();
        } else {
          connectToWiFi();
        }
      }
    }
    return;
  }

  if (currentState == STATE_ENTER_PASSWORD) {
    wifiTouchUI_handleKeyboardTouch(x, y, tft);
    return;
  }

  if (currentState == STATE_CONNECTED) {
    if (y >= 280 && y <= 310) {
      WiFi.disconnect();
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList(tft);
    }
    return;
  }

  if (currentState == STATE_FAILED) {
    currentState = STATE_SCAN_NETWORKS;
    drawNetworkList(tft);
    return;
  }

  if (currentState == STATE_SELECT_LOCATION) {
    wifiTouchUI_handleLocationTouch(x, y, tft);
    return;
  }

  if (currentState == STATE_ENTER_COORDINATES) {
    wifiTouchUI_handleCoordinatesTouch(x, y, tft);
    return;
  }

  if (currentState == STATE_CONFIG_MODE) {
    Serial.printf("CONFIG MODE TOUCH: X=%d, Y=%d\n", x, y);

    if (x >= 220 && x <= 315 && y >= 5 && y <= 29) {
      Serial.println("BTN: TOUCH CALIBRATION");
      tft.fillRect(220, 5, 95, 24, YELLOW);
      tft.setTextColor(BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("START...", 220 + 95 / 2, 5 + 12);
      tft.setTextDatum(TL_DATUM);
      delay(250);

      runTouchCalibrationFromMenu(tft);
      drawConfigModeScreen();
      return;
    }

    if (y >= 35 && y <= 180) {
      int selectedIndex = (y - 35) / 25;
      if (selectedIndex >= 0 && selectedIndex < min(networkCount, 5)) {
        selectedNetworkIndex = selectedIndex;
        currentSSID = networkNames[selectedIndex];

        tft.fillRect(0, 35 + selectedIndex * 25, 320, 25, GREEN);
        delay(300);

        if (networkSecure[selectedIndex]) {
          currentState = STATE_ENTER_PASSWORD;
          enteredPassword = "";
          drawPasswordScreen();
        } else {
          connectToWiFi();
        }
      }
      return;
    }

    if (y >= 190 && y <= 240) {
      int btnW = 75;
      int gap = 3;

      if (x >= 2 && x <= 2 + btnW) {
        Serial.println("BTN: REFRESH");
        tft.fillRect(2, 190, btnW, 45, YELLOW);
        tft.setTextColor(BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("SCAN...", 2 + btnW / 2, 190 + 22);
        tft.setTextDatum(TL_DATUM);
        delay(300);

        selectedNetworkIndex = -1;
        scanNetworks();
        drawConfigModeScreen();
        return;
      }

      if (x >= 2 + btnW + gap && x <= 2 + 2 * btnW + gap) {
        Serial.println("BTN: LOCATION");
        tft.fillRect(2 + btnW + gap, 190, btnW, 45, YELLOW);
        delay(300);
        enterLocationSelectionMode(tft);
        return;
      }

      if (x >= 2 + 2 * (btnW + gap) && x <= 2 + 3 * btnW + 2 * gap) {
        if (isOfflineMode) {
          Serial.println("BTN: SWITCHING TO ONLINE");

          tft.fillRect(2 + 2 * (btnW + gap), 190, btnW, 45, GREEN);
          tft.setTextColor(BLACK);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("WIFI ON", 2 + 2 * (btnW + gap) + btnW / 2, 190 + 22);
          delay(200);

          isOfflineMode = false;
          saveOfflineModePref(false);
          WiFi.mode(WIFI_STA);

          Preferences p;
          p.begin("wifi", false);
          String savedSSID = p.getString("ssid", "");
          String savedPass = p.getString("password", "");
          p.end();

          if (savedSSID.length() > 0) {
            Serial.printf("[ONLINE] Trying saved WiFi: '%s'\n", savedSSID.c_str());
            tft.fillScreen(BLACK);
            tft.setTextColor(WHITE);
            tft.setTextSize(2);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("Wznawianie WiFi...", tft.width() / 2, tft.height() / 2 - 20);
            tft.setTextSize(1);
            tft.drawString(savedSSID, tft.width() / 2, tft.height() / 2 + 10);

            WiFi.begin(savedSSID.c_str(), savedPass.c_str());

            int wait = 0;
            while (WiFi.status() != WL_CONNECTED && wait < 30) {
              delay(500);
              Serial.print(".");
              wait++;
            }

            if (WiFi.status() == WL_CONNECTED) {
              Serial.println("\nUdalo sie polaczyc!");
              Serial.println("[ONLINE] Connected using saved WiFi");
              onWiFiConnectedTasks();

              currentState = STATE_CONNECTED;
              tft.fillScreen(COLOR_BACKGROUND);
              wifiLostDetected = false;

              extern ScreenManager& getScreenManager();
              getScreenManager().resetScreenTimer();
              getScreenManager().forceScreenRefresh(tft);
              return;
            }

            Serial.println("\nBlad polaczenia - skanowanie...");
            Serial.println("[ONLINE] Failed to connect saved WiFi -> scanning networks");
            currentState = STATE_SCAN_NETWORKS;
            scanNetworks();
            drawNetworkList(tft);

            tft.fillRect(10, 210, 300, 25, RED);
            tft.setTextColor(WHITE);
            tft.setCursor(20, 220);
            tft.print("Blad polaczenia - wybierz siec");
            return;
          }

          Serial.println("Brak zapisanej sieci, skanuje...");
          Serial.println("[ONLINE] No saved WiFi -> scanning networks");
          currentState = STATE_SCAN_NETWORKS;
          scanNetworks();
          drawNetworkList(tft);
          return;
        }

        // switch offline
        Serial.println("BTN: SWITCHING TO OFFLINE (Config Mode)");

        tft.fillRect(2 + 2 * (btnW + gap), 190, btnW, 45, ORANGE);
        tft.setTextColor(BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("OFF..", 2 + 2 * (btnW + gap) + btnW / 2, 190 + 22);
        delay(500);

        isOfflineMode = true;
        saveOfflineModePref(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);

        currentState = STATE_CONNECTED;
        tft.fillScreen(BLACK);
        wifiLostDetected = false;

        extern ScreenManager& getScreenManager();
        getScreenManager().resetScreenTimer();
        getScreenManager().setCurrentScreen(SCREEN_LOCAL_SENSORS);
        forceScreenRefresh(tft);
        return;
      }

      if (x >= 2 + 3 * (btnW + gap)) {
        Serial.println("BTN: EXIT");
        tft.fillRect(2 + 3 * (btnW + gap), 190, btnW, 45, YELLOW);
        delay(100);

        Preferences p;
        p.begin("wifi", false);
        String savedSSID = p.getString("ssid", "");
        String savedPass = p.getString("password", "");
        p.end();

        if (savedSSID.length() > 0) {
          tft.fillScreen(BLACK);
          tft.setTextColor(WHITE);
          tft.setTextSize(2);
          tft.setTextDatum(MC_DATUM);
          tft.drawString("Wznawianie...", tft.width() / 2, tft.height() / 2 - 20);
          tft.setTextSize(1);
          tft.drawString(savedSSID, tft.width() / 2, tft.height() / 2 + 10);

          WiFi.begin(savedSSID.c_str(), savedPass.c_str());

          int wait = 0;
          while (WiFi.status() != WL_CONNECTED && wait < 8) {
            delay(500);
            wait++;
            Serial.print(".");
          }
        }

        currentState = STATE_CONNECTED;
        tft.fillScreen(BLACK);

        if (WiFi.status() == WL_CONNECTED) {
          wifiLostDetected = false;
          wifiLostTime = 0;
          reconnectAttemptInProgress = false;
          backgroundReconnectActive = false;

          // After exiting config mode, show loading UI for a short window
          extern bool isWeatherRefreshInProgress;
          extern unsigned long weatherRefreshStartMs;
          extern unsigned long weatherRefreshTimeoutMs;
          isWeatherRefreshInProgress = true;
          weatherRefreshStartMs = millis();
          weatherRefreshTimeoutMs = 5000UL;

          extern ScreenManager& getScreenManager();
          getScreenManager().resetScreenTimer();
          getScreenManager().forceScreenRefresh(tft);
        } else {
          Serial.println("Nie udalo sie wznowic polaczenia przy wyjsciu.");
          wifiLostDetected = true;
          wifiLostTime = millis();
          lastReconnectAttempt = millis();
          wifiWasConnected = true;
        }
        return;
      }
    }
  }
}
