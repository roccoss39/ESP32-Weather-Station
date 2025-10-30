#include "display/weather_icons.h"
#include "config/display_config.h"

void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode) {
  tft.fillRect(x, y, ICON_SIZE, ICON_SIZE, COLOR_BACKGROUND); // Wyczyść obszar ikony
  
  Serial.println("Rysowanie ikony dla: main='" + condition + "', icon='" + iconCode + "'");
  
  // Ikony na podstawie oficjalnej listy OpenWeatherMap
  // Group 800: Clear
  if (iconCode.indexOf("01") >= 0 || condition == "clear sky") { // 01d, 01n = clear sky
    // Słońce - czyste niebo
    tft.fillCircle(x + 25, y + 25, 15, TFT_YELLOW);
    for (int i = 0; i < 8; i++) {
      float angle = i * 45 * PI / 180;
      int x1 = x + 25 + cos(angle) * 20;
      int y1 = y + 25 + sin(angle) * 20;
      int x2 = x + 25 + cos(angle) * 25;
      int y2 = y + 25 + sin(angle) * 25;
      tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    }
  }
  // Group 80x: Clouds  
  else if (iconCode.indexOf("02") >= 0 || iconCode.indexOf("03") >= 0 || iconCode.indexOf("04") >= 0 ||
           condition.indexOf("clouds") >= 0) {
    // 02d/02n = few clouds, 03d/03n = scattered clouds, 04d/04n = broken/overcast clouds
    tft.fillCircle(x + 15, y + 30, 12, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 25, 15, TFT_WHITE);
    tft.fillCircle(x + 35, y + 30, 12, TFT_LIGHTGREY);
    tft.fillRect(x + 10, y + 35, 30, 8, TFT_WHITE);
  }
  // Group 2xx: Thunderstorm
  else if (iconCode.indexOf("11") >= 0 || condition.indexOf("thunderstorm") >= 0) {
    // 11d/11n = thunderstorm (codes 200-232)
    // Ciemne chmury burzy
    tft.fillCircle(x + 15, y + 20, 10, TFT_DARKGREY);
    tft.fillCircle(x + 25, y + 15, 12, TFT_LIGHTGREY);
    tft.fillCircle(x + 35, y + 20, 10, TFT_DARKGREY);
    tft.fillRect(x + 12, y + 25, 26, 6, TFT_DARKGREY);
    // Błyskawica
    tft.drawLine(x + 20, y + 32, x + 25, y + 38, TFT_YELLOW);
    tft.drawLine(x + 25, y + 38, x + 22, y + 42, TFT_YELLOW);
    tft.drawLine(x + 22, y + 42, x + 27, y + 47, TFT_YELLOW);
    // Krople deszczu burzy
    for (int i = 0; i < 3; i++) {
      tft.drawLine(x + 13 + i * 6, y + 33, x + 13 + i * 6, y + 39, TFT_CYAN);
    }
  }
  // Group 3xx: Drizzle + Group 5xx: Rain
  else if (iconCode.indexOf("09") >= 0 || iconCode.indexOf("10") >= 0 || 
           condition.indexOf("drizzle") >= 0 || condition.indexOf("rain") >= 0 ||
           condition.indexOf("mzawka") >= 0 || condition.indexOf("deszcz") >= 0 || condition.indexOf("opady") >= 0) {
    // 09d/09n = drizzle/shower rain, 10d/10n = rain
    tft.fillCircle(x + 15, y + 20, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 15, 12, TFT_WHITE);
    tft.fillCircle(x + 35, y + 20, 10, TFT_LIGHTGREY);
    tft.fillRect(x + 12, y + 25, 26, 6, TFT_WHITE);
    // Krople deszczu
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 15 + i * 5, y + 32, x + 15 + i * 5, y + 40, TFT_CYAN);
    }
  }
  // Group 6xx: Snow
  else if (iconCode.indexOf("13") >= 0 || condition.indexOf("snow") >= 0 || condition.indexOf("sleet") >= 0 ||
           condition.indexOf("snieg") >= 0 || condition.indexOf("krupa") >= 0 || condition.indexOf("sniezyca") >= 0) {
    // 13d/13n = snow (codes 600-622)
    tft.fillCircle(x + 15, y + 20, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 15, 12, TFT_WHITE);
    tft.fillCircle(x + 35, y + 20, 10, TFT_LIGHTGREY);
    tft.fillRect(x + 12, y + 25, 26, 6, TFT_WHITE);
    // Płatki śniegu
    for (int i = 0; i < 3; i++) {
      int sx = x + 18 + i * 8;
      int sy = y + 35;
      tft.drawPixel(sx, sy, TFT_WHITE);
      tft.drawPixel(sx-1, sy, TFT_WHITE);
      tft.drawPixel(sx+1, sy, TFT_WHITE);
      tft.drawPixel(sx, sy-1, TFT_WHITE);
      tft.drawPixel(sx, sy+1, TFT_WHITE);
    }
  }
  // Group 7xx: Atmosphere  
  else if (iconCode.indexOf("50") >= 0 || condition == "mist" || condition == "smoke" || 
           condition == "haze" || condition == "dust" || condition == "fog" || 
           condition == "sand" || condition == "ash" || condition == "squall" || condition == "tornado" ||
           condition == "mgla" || condition == "dym" || condition == "zamglenie" || condition == "pyl" || 
           condition == "piasek" || condition == "popiol" || condition == "szkwaly") {
    // 50d/50n = mist, fog, smoke, haze, dust, sand, ash, squall, tornado (codes 701-781)
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 5, y + 20 + i * 5, x + 45, y + 20 + i * 5, TFT_LIGHTGREY);
    }
  }
  else {
    // Domyślna ikona - znak zapytania
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.setTextSize(3);
    tft.drawString("?", x + 20, y + 15);
  }
}