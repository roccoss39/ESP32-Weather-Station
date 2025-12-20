// #include "managers/ScreenManager.h"
// #include "display/screen_manager.h"
// #include "display/weather_display.h"
// #include "display/forecast_display.h"
// #include "display/time_display.h"
// #include "display/github_image.h"
// #include "config/display_config.h"
// #include "sensors/motion_sensor.h"
// #include "sensors/dht22_sensor.h"
// #include "managers/WeatherCache.h"
// #include "managers/TimeDisplayCache.h"
// #include "config/location_config.h"
// #include "config/hardware_config.h"
// #include "weather/forecast_data.h"

// // --- ZMIENNE ZEWNĘTRZNE ---
// extern WeeklyForecastData weeklyForecast;
// extern ForecastData forecast;
// extern unsigned long lastWeatherCheckGlobal;
// extern bool isOfflineMode;

// extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);

// // --- SINGLETON ---
// static ScreenManager screenManager;

// ScreenManager& getScreenManager() {
//     return screenManager;
// }

// void updateScreenManager() {
//     if (getDisplayState() == DISPLAY_SLEEPING) return;
//     getScreenManager().updateScreenManager();
// }

// void switchToNextScreen(TFT_eSPI& tft) {
//     getScreenManager().renderCurrentScreen(tft);
// }

// void forceScreenRefresh(TFT_eSPI& tft) {
//     getScreenManager().forceScreenRefresh(tft);
// }

// // =========================================================
// // === FUNKCJE POMOCNICZE (STATYCZNE) ===
// // =========================================================

// static String local_shortenDescription(String description) {
//     String polishDescription = description;
//     if (description.indexOf("thunderstorm with heavy rain") >= 0) polishDescription = "Burza z ulewa";
//     else if (description.indexOf("thunderstorm with rain") >= 0) polishDescription = "Burza z deszczem";
//     else if (description.indexOf("thunderstorm") >= 0) polishDescription = "Burza";
//     else if (description.indexOf("drizzle") >= 0) polishDescription = "Mzawka";
//     else if (description.indexOf("heavy intensity rain") >= 0) polishDescription = "Ulewa";
//     else if (description.indexOf("moderate rain") >= 0) polishDescription = "Umiarkowany deszcz";
//     else if (description.indexOf("light rain") >= 0) polishDescription = "Slaby deszcz";
//     else if (description.indexOf("rain") >= 0) polishDescription = "Deszcz";
//     else if (description.indexOf("snow") >= 0) polishDescription = "Snieg";
//     else if (description.indexOf("sleet") >= 0) polishDescription = "Deszcz ze sniegiem";
//     else if (description.indexOf("mist") >= 0) polishDescription = "Mgla";
//     else if (description.indexOf("fog") >= 0) polishDescription = "Mgla";
//     else if (description.indexOf("clear sky") >= 0) polishDescription = "Bezchmurnie";
//     else if (description.indexOf("overcast clouds") >= 0) polishDescription = "Pochmurno";
//     else if (description.indexOf("broken clouds") >= 0) polishDescription = "Duze zachmurzenie";
//     else if (description.indexOf("scattered clouds") >= 0) polishDescription = "Srednie zachmurzenie";
//     else if (description.indexOf("few clouds") >= 0) polishDescription = "Male zachmurzenie";
//     return polishDescription;
// }

// static uint16_t local_getWindColor(float windKmh) {
//     if (windKmh >= 30.0) return TFT_MAROON;
//     else if (windKmh >= 25.0) return TFT_RED;
//     else if (windKmh >= 20.0) return TFT_YELLOW;
//     else return TFT_WHITE;
// }

// static uint16_t local_getPressureColor(float pressure) {
//     if (pressure < 1000.0) return TFT_ORANGE;
//     else if (pressure > 1020.0) return TFT_MAGENTA;
//     else return TFT_WHITE;
// }

// static uint16_t local_getHumidityColor(float humidity) {
//     if (humidity < 30.0) return TFT_RED;
//     else if (humidity > 90.0) return 0x7800;
//     else if (humidity > 85.0) return TFT_PURPLE;
//     else return TFT_WHITE;
// }

// // =========================================================
// // === EKRAN 1: POGODA BIEŻĄCA (NOWY DESIGN - GRAFIT) ===
// // =========================================================

// void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
//     tft.fillScreen(COLOR_BACKGROUND);
//     displayTime(tft);

//     if (!weather.isValid) {
//         tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
//         tft.setTextDatum(MC_DATUM);
//         tft.setTextFont(1);
//         tft.setTextSize(2);
//         tft.drawString("BRAK DANYCH", 160, 100);
//         return;
//     }

//     tft.setTextFont(1);
//     int startY = 40;
//     int height = 140;

//     // STYL: Ciemny Grafit (0x1082)
//     uint16_t CARD_BG = 0x1082;
//     uint16_t TEXT_BG = CARD_BG;
//     uint16_t BORDER_COLOR = TFT_DARKGREY;
//     uint16_t LABEL_COLOR = TFT_SILVER;

//     // --- LEWA KARTA ---
//     tft.fillRoundRect(5, startY, 150, height, 8, CARD_BG);
//     tft.drawRoundRect(5, startY, 150, height, 8, BORDER_COLOR);

//     String polishDesc = local_shortenDescription(weather.description);
//     polishDesc.toUpperCase();

//     drawWeatherIcon(tft, 80, startY + 20, weather.description, weather.icon);

//     uint16_t tempColor = TFT_WHITE;
//     if (weather.temperature < 0 || weather.feelsLike < 0) tempColor = TFT_CYAN;
//     else if (weather.temperature > 30) tempColor = TFT_RED;
//     else if (weather.temperature > 25) tempColor = TFT_ORANGE;

//     tft.setTextColor(tempColor, TEXT_BG);
//     tft.setTextSize(5);
//     tft.setTextDatum(MC_DATUM);
//     String tempStr = String((int)round(weather.temperature));
//     tft.drawString(tempStr, 80, startY + 60);

//     tft.setTextSize(2);
//     int tempWidth = tft.textWidth(tempStr) * 5;
//     tft.drawString("C", 80 + (tempWidth/2) + 10, startY + 50);

//     uint16_t descColor = TFT_CYAN;
//     if (polishDesc.indexOf("BURZA") >= 0) descColor = TFT_RED;
//     else if (polishDesc == "BEZCHMURNIE") descColor = TFT_YELLOW;
//     else if (polishDesc == "MGLA") descColor = TFT_WHITE;

//     tft.setTextColor(descColor, TEXT_BG);
//     tft.setTextSize(2);
//     if (tft.textWidth(polishDesc) > 140) tft.setTextSize(1);
//     tft.setTextDatum(MC_DATUM);
//     tft.drawString(polishDesc, 80, startY + 95);

//     tft.setTextColor(LABEL_COLOR, TEXT_BG);
//     tft.setTextSize(1);
//     tft.setTextDatum(MC_DATUM);
//     tft.drawString("ODCZUWALNA:", 80, startY + 116);
//     tft.setTextColor(TFT_WHITE, TEXT_BG);
//     tft.setTextSize(2);
//     tft.drawString(String((int)round(weather.feelsLike)) + " C", 80, startY + 130);

//     // --- PRAWA KOLUMNA ---
//     int rowH = 42; int gap = 6; int rightX = 165; int rightW = 150;

//     // 1. Wilgotność
//     int y1 = startY;
//     tft.fillRoundRect(rightX, y1, rightW, rowH, 6, CARD_BG);
//     tft.drawRoundRect(rightX, y1, rightW, rowH, 6, BORDER_COLOR);
//     tft.setTextDatum(TL_DATUM);
//     tft.setTextColor(LABEL_COLOR, TEXT_BG);
//     tft.setTextSize(1);
//     tft.drawString("WILGOTNOSC", rightX + 5, y1 + 5);

//     tft.setTextColor(LABEL_COLOR, TEXT_BG);
//     tft.setTextSize(1);
//     tft.drawString("OPADY:", rightX + 5, y1 + 25);
//     String rainVal = "--";
//     uint16_t rainColor = LABEL_COLOR;
//     if (forecast.isValid && forecast.count > 0) {
//         int chance = forecast.items[0].precipitationChance;
//         rainVal = String(chance) + "%";
//         rainColor = (chance > 0) ? TFT_SKYBLUE : LABEL_COLOR;
//     }
//     tft.setTextColor(rainColor, TEXT_BG);
//     tft.setTextSize(2);
//     tft.drawString(rainVal, rightX + 50, y1 + 20);

//     tft.setTextDatum(TR_DATUM);
//     tft.setTextColor(local_getHumidityColor(weather.humidity), TEXT_BG);
//     tft.setTextSize(3);
//     tft.drawString(String((int)weather.humidity) + "%", rightX + rightW - 5, y1 + 12);

//     // 2. Wiatr
//     int y2 = y1 + rowH + gap;
//     tft.fillRoundRect(rightX, y2, rightW, rowH, 6, CARD_BG);
//     tft.drawRoundRect(rightX, y2, rightW, rowH, 6, BORDER_COLOR);
//     tft.setTextDatum(TL_DATUM);
//     tft.setTextColor(LABEL_COLOR, TEXT_BG);
//     tft.setTextSize(1);
//     tft.drawString("WIATR", rightX + 5, y2 + 5);
//     float windKmh = weather.windSpeed * 3.6;
//     tft.setTextDatum(TR_DATUM);
//     tft.setTextColor(local_getWindColor(windKmh), TEXT_BG);
//     tft.setTextSize(3);
//     tft.drawString(String((int)round(windKmh)), rightX + rightW - 35, y2 + 12);
//     tft.setTextSize(1);
//     tft.setTextColor(TFT_WHITE, TEXT_BG);
//     tft.drawString("km/h", rightX + rightW - 5, y2 + 22);

//     // 3. Ciśnienie
//     int y3 = y2 + rowH + gap;
//     tft.fillRoundRect(rightX, y3, rightW, rowH, 6, CARD_BG);
//     tft.drawRoundRect(rightX, y3, rightW, rowH, 6, BORDER_COLOR);
//     tft.setTextDatum(TL_DATUM);
//     tft.setTextColor(LABEL_COLOR, TEXT_BG);
//     tft.setTextSize(1);
//     tft.drawString("CISNIENIE", rightX + 5, y3 + 5);
//     tft.setTextDatum(TR_DATUM);
//     tft.setTextColor(local_getPressureColor(weather.pressure), TEXT_BG);
//     tft.setTextSize(3);
//     if (weather.pressure > 999) tft.setTextSize(2);
//     tft.drawString(String((int)weather.pressure), rightX + rightW - 30, y3 + 12);
//     tft.setTextSize(1);
//     tft.setTextColor(TFT_WHITE, TEXT_BG);
//     tft.drawString("hPa", rightX + rightW - 5, y3 + 22);

//     tft.setTextSize(1);
//     updateWeatherCache();
// }

// // =========================================================
// // === EKRAN 2: PROGNOZA TYGODNIOWA ===
// // =========================================================

// void ScreenManager::renderWeeklyScreen(TFT_eSPI& tft) {
//     tft.fillScreen(COLOR_BACKGROUND);

//     if (!weeklyForecast.isValid) {
//         tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
//         tft.setTextDatum(MC_DATUM);
//         tft.drawString("BRAK DANYCH", 160, 120);
//         return;
//     }

//     tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
//     tft.setTextSize(2);
//     tft.setTextDatum(TC_DATUM);
//     tft.drawString("PROGNOZA TYGODNIOWA", 160, 10);

//     int y = 50;
//     tft.setTextSize(1);

//     for(int i=0; i<4; i++) {
//         String line = weeklyForecast.days[i].dayName + ": " +
//                       String((int)weeklyForecast.days[i].tempMax) + "/" +
//                       String((int)weeklyForecast.days[i].tempMin) + "C";

//         tft.setTextDatum(TL_DATUM);
//         tft.drawString(line, 20, y);

//         String desc = weeklyForecast.days[i].description;
//         tft.drawString(desc, 150, y);

//         y += 40;
//     }
// }

// // =========================================================
// // === EKRAN 3: SENSORY LOKALNE ===
// // =========================================================

// void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
//     tft.fillScreen(COLOR_BACKGROUND);

//     tft.setTextColor(TFT_ORANGE, COLOR_BACKGROUND);
//     tft.setTextSize(2);
//     tft.setTextDatum(MC_DATUM);
//     tft.drawString("SENSORY LOKALNE", 160, 40);

//     DHT22Data data = getDHT22Data();

//     if (data.isValid) {
//         tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
//         tft.setTextSize(3);
//         String t = "Temp: " + String(data.temperature, 1) + " C";
//         tft.drawString(t, 160, 100);
//         String h = "Wilg: " + String((int)data.humidity) + " %";
//         tft.drawString(h, 160, 150);
//     } else {
//         tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
//         tft.drawString("BLAD ODCZYTU", 160, 120);
//     }
// }

// void ScreenManager::renderForecastScreen(TFT_eSPI& tft) { }
// void ScreenManager::renderImageScreen(TFT_eSPI& tft) { displayGitHubImage(tft); }
// void ScreenManager::resetWeatherAndTimeCache() { getWeatherCache().resetCache(); getTimeDisplayCache().resetCache(); }