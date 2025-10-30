#include "display/weather_icons.h"
#include "config/display_config.h"

void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode) {
  tft.fillRect(x, y, ICON_SIZE, ICON_SIZE, COLOR_BACKGROUND); // Wyczyść obszar ikony
  
  Serial.println("Rysowanie ikony dla: opis='" + condition + "', kod='" + iconCode + "'");
  
  // Ikony na podstawie kodu API (bardziej precyzyjne)
  if (iconCode.indexOf("01") >= 0) { // 01d, 01n = clear sky
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
  else if (iconCode.indexOf("02") >= 0 || iconCode.indexOf("03") >= 0 || iconCode.indexOf("04") >= 0 || 
           condition.indexOf("chmur") >= 0 || condition.indexOf("pochmur") >= 0) {
    // 02d/02n = few clouds, 03d/03n = scattered clouds, 04d/04n = broken clouds
    tft.fillCircle(x + 15, y + 30, 12, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 25, 15, TFT_WHITE);
    tft.fillCircle(x + 35, y + 30, 12, TFT_LIGHTGREY);
    tft.fillRect(x + 10, y + 35, 30, 8, TFT_WHITE);
  }
  else if (iconCode.indexOf("09") >= 0 || iconCode.indexOf("10") >= 0 || 
           condition.indexOf("deszcz") >= 0 || condition.indexOf("opad") >= 0) {
    // 09d/09n = shower rain, 10d/10n = rain
    tft.fillCircle(x + 15, y + 20, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 15, 12, TFT_WHITE);
    tft.fillCircle(x + 35, y + 20, 10, TFT_LIGHTGREY);
    tft.fillRect(x + 12, y + 25, 26, 6, TFT_WHITE);
    // Krople deszczu
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 15 + i * 5, y + 32, x + 15 + i * 5, y + 40, TFT_CYAN);
    }
  }
  else if (iconCode.indexOf("13") >= 0 || condition.indexOf("śnieg") >= 0 || condition.indexOf("snieg") >= 0) {
    // 13d/13n = snow
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
  else if (iconCode.indexOf("50") >= 0 || condition.indexOf("mgła") >= 0 || condition.indexOf("zamgl") >= 0 || condition.indexOf("mgla") >= 0) {
    // 50d/50n = mist/fog
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