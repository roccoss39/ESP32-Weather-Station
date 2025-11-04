#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <TFT_eSPI.h>

// --- KOLORY ---
#define COLOR_BACKGROUND    TFT_BLACK
#define COLOR_TEMPERATURE   TFT_ORANGE
#define COLOR_TEMPERATURE_COLD TFT_BLUE   // Niebieski dla temp < 0°C
#define COLOR_DESCRIPTION   TFT_CYAN
#define COLOR_DESCRIPTION_STORM TFT_MAROON // Ciemny czerwony dla burzy
#define COLOR_DESCRIPTION_SUNNY TFT_YELLOW // Żółty dla słońca
#define COLOR_DESCRIPTION_FOG   TFT_WHITE  // Biały dla mgły
#define COLOR_HUMIDITY      TFT_WHITE
#define COLOR_TIME          TFT_YELLOW
#define COLOR_DATE          TFT_WHITE
#define COLOR_DAY           TFT_LIGHTGREY
#define COLOR_WIFI_OK       TFT_GREEN
#define COLOR_WIFI_ERROR    TFT_RED

// --- KOLORY WIATRU (według prędkości km/h) ---
#define COLOR_WIND_CALM     TFT_WHITE      // 0-15 km/h - normalny
#define COLOR_WIND_MODERATE TFT_YELLOW     // 15-20 km/h - umiarkowany
#define COLOR_WIND_STRONG   TFT_RED        // 20-25 km/h - silny
#define COLOR_WIND_EXTREME  TFT_MAROON     // 25+ km/h - bardzo silny

// --- KOLORY CIŚNIENIA (według hPa) ---
#define COLOR_PRESSURE_LOW    TFT_ORANGE   // <1000 hPa - niskie (deszcz)
#define COLOR_PRESSURE_NORMAL TFT_WHITE    // 1000-1020 hPa - normalne
#define COLOR_PRESSURE_HIGH   TFT_MAGENTA  // >1020 hPa - wysokie (pogodnie)

// --- KOLORY WILGOTNOŚCI (według %) ---
#define COLOR_HUMIDITY_DRY     TFT_MAROON  // <30% - za sucho (ciemny czerwony)
#define COLOR_HUMIDITY_COMFORT TFT_WHITE   // 30-70% - komfort  
#define COLOR_HUMIDITY_HUMID   TFT_PURPLE  // 70-85% - podwyższona (unikalny fioletowy)
#define COLOR_HUMIDITY_WET     TFT_BROWN   // >85% - bardzo wilgotno (brązowy - pleśń)

// --- POZYCJE I ROZMIARY ---
#define WEATHER_AREA_X      5
#define WEATHER_AREA_Y      5
#define WEATHER_AREA_WIDTH  310
#define WEATHER_AREA_HEIGHT 140

#define TIME_AREA_X         5
#define TIME_AREA_Y         190
#define TIME_AREA_OFFSET_Y  25
#define TIME_AREA_WIDTH     310
#define TIME_AREA_HEIGHT    85

#define ICON_SIZE           50
#define ICON_X_OFFSET       5
#define ICON_Y_OFFSET       0

#define TEMP_X_OFFSET       60
#define TEMP_Y_OFFSET       20

#define DESC_X_OFFSET       5
#define DESC_Y_OFFSET       55

#define HUMIDITY_X_OFFSET   5
#define HUMIDITY_Y_OFFSET   85

#define WIND_X_OFFSET       5
#define WIND_Y_OFFSET       115

#define PRESSURE_X_OFFSET   5
#define PRESSURE_Y_OFFSET   145

// --- ROZMIARY CZCIONEK ---
#define FONT_SIZE_LARGE     3
#define FONT_SIZE_MEDIUM    2
#define FONT_SIZE_SMALL     1

// --- TYPY EKRANÓW ---
enum ScreenType {
  SCREEN_CURRENT_WEATHER = 0,
  SCREEN_FORECAST = 1,
  SCREEN_IMAGE = 2
};

#endif