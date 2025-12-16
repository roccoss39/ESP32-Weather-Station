#ifndef TIMING_CONFIG_H
#define TIMING_CONFIG_H

/**
 * 🕐 CENTRALNA KONFIGURACJA TIMEOUTÓW I OPÓŹNIEŃ
 * 
 * Wszystkie magic numbers zebrane w jednym miejscu dla łatwego zarządzania
 * i konsystencji w całym projekcie.
 */

// === WIFI TOUCH INTERFACE TIMEOUTS ===
#define WIFI_LOSS_TIMEOUT           60000   // 60s - czas po którym przejdzie do scan mode
#define WIFI_CONFIG_MODE_TIMEOUT    180000  // 180s (3 min) - timeout config mode
#define WIFI_LONG_PRESS_TIME        3000    // 3s - czas długiego naciśnięcia
#define WIFI_RECONNECT_INTERVAL     9000   // 9s - interval między próbami reconnect
#define WIFI_CONNECTION_TIMEOUT     10000   // 10s - timeout pojedynczej próby połączenia
#define WIFI_STATUS_CHECK_INTERVAL  2000    // 2s - jak często sprawdzać status WiFi
#define WIFI_FINAL_WAIT_TIME        3000    // 3s - ostatnie oczekiwanie przed scan mode
#define WIFI_UI_UPDATE_INTERVAL     5000    // 5s - aktualizacja UI countdown
#define WIFI_PROGRESS_START_TIME    1000    // 1s - kiedy zacząć pokazywać progress bar

// === DELAY TIMES (blocking operations) ===
#define DELAY_STABILIZATION         1000    // 1s - stabilizacja po wake up
#define DELAY_CONNECTION_STEP       500     // 500ms - między próbami połączenia  
#define DELAY_ERROR_DISPLAY         2000    // 2s - pokazanie błędu
#define DELAY_SUCCESS_DISPLAY       3000    // 3s - pokazanie sukcesu
#define DELAY_VISUAL_FEEDBACK       500     // 500ms - wizualna odpowiedź na dotyk
#define DELAY_WAKE_UP_MESSAGE       2000    // 2s - pokazanie wake up message
#define DELAY_SLEEP_MESSAGE         3000    // 3s - pokazanie sleep message

// === WEATHER API INTERVALS ===
#define WEATHER_UPDATE_NORMAL       600000  // 10 min - normalny interval pogody
#define WEATHER_UPDATE_ERROR        10000   // 20s - interval po błędzie
#define WEATHER_FORCE_REFRESH       10000   // 20s - wymuszenie odświeżenia po błędzie
#define WEEKLY_UPDATE_INTERVAL     14400000  // 4 godziny - weekly forecast update
#define WEEKLY_UPDATE_ERROR         5000    // 5s - weekly forecast po błędzie/zmianie lokalizacji

// === DISPLAY & SCREEN TIMEOUTS ===
// NOTE: SCREEN_SWITCH_INTERVAL jest już zdefiniowany w ScreenManager.h
#define DISPLAY_UPDATE_INTERVAL     1000    // 1s - aktualizacja wyświetlacza
#define MOTION_CONFIG_TIMEOUT       600000  // 10 min - timeout w config mode (PIR)

// === COUNTDOWN & UI UPDATE INTERVALS ===
#define COUNTDOWN_UPDATE_INTERVAL   1000    // 1s - aktualizacja countdown na UI

#endif