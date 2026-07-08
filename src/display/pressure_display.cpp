#include "display/display_pressure.h"
#include "display_utils.h"
#include "weather/open_meteo_api.h"



void displayPressureScreen(TFT_eSPI& tft) {
    // Kolory interfejsu (Format RGB565)
    Serial.print("Rysowanie Pressure Screen\n");
    uint16_t COLOR_BG = TFT_BLACK;
    uint16_t COLOR_GRID = tft.color565(50, 50, 50);
    uint16_t COLOR_TEXT_MUTED = tft.color565(150, 150, 150);
    uint16_t COLOR_BAR = tft.color565(80, 130, 180);
    uint16_t COLOR_BAR_NOW = tft.color565(0, 200, 255);
    uint16_t COLOR_WIDGET_BG = tft.color565(20, 25, 30);
    uint16_t COLOR_WIDGET_BORDER = tft.color565(80, 80, 80);

    // 1. TŁO I TYTUŁ
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
    tft.setTextDatum(TC_DATUM); 
    tft.setTextSize(2);
    tft.drawString("Cisnienie Atmosferyczne", tft.width() / 2, 8);
    tft.drawFastHLine(20, 30, tft.width() - 40, COLOR_GRID);

    // ==========================================
    // 2. SPRAWDZENIE I POBRANIE DANYCH Z API
    // ==========================================
    float drawHistory[12];

    if (!isOpenMeteoDataValid()) {
        // Brak danych z Open-Meteo (np. brak połączenia z siecią)
        clearAndShowMessage(tft, "Oczekiwanie na\ndane z internetu...", TFT_WHITE, 2);
        return;
    }

    // Pobieramy historię bezpośrednio ze wskaźnika API
    const float* onlineData = getOpenMeteoPressureHistory();
    for (int i = 0; i < 12; i++) {
        drawHistory[i] = onlineData[i];
    }

    // ==========================================
    // 3. DYNAMICZNA SKALA
    // ==========================================
    float minP = drawHistory[0];
    float maxP = drawHistory[0];
    
    for (int i = 1; i < 12; i++) {
        if (drawHistory[i] < minP) minP = drawHistory[i];
        if (drawHistory[i] > maxP) maxP = drawHistory[i];
    }

    if (maxP - minP < 0.5) {
        maxP += 1.0;
        minP -= 1.0;
    } else {
        float padding = (maxP - minP) * 0.15;
        maxP += padding;
        minP -= padding;
    }

    // ==========================================
    // 4. WYMIARY WYKRESU 
    // ==========================================
    int xOffset = 45; 
    int yOffset = 45; 
    int chartW = tft.width() - xOffset - 15; 
    int chartH = tft.height() - yOffset - 55; 
    int chartBottom = yOffset + chartH;
    int maxSlots = 12; 

    // ==========================================
    // 5. RYSOWANIE SIATKI (GRID)
    // ==========================================
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM); 
    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);

    for (int i = 0; i <= 3; i++) {
        float val = maxP - i * ((maxP - minP) / 3.0);
        int y = yOffset + i * (chartH / 3);
        
        for (int dx = xOffset; dx < xOffset + chartW; dx += 5) {
            tft.drawPixel(dx, y, COLOR_GRID);
        }
        tft.drawString(String(val, 1), xOffset - 5, y);
    }

    // ==========================================
    // 6. RYSOWANIE SŁUPKÓW I OSI X
    // ==========================================
    int barSpacing = 4; 
    int barW = (chartW / maxSlots) - barSpacing;

    tft.setTextDatum(TC_DATUM); 

    for (int i = 0; i < 12; i++) {
        int barX = xOffset + (i * (chartW / maxSlots)) + (barSpacing / 2);
        
        float normalizedValue = (drawHistory[i] - minP) / (maxP - minP); 
        int barH = (int)(normalizedValue * chartH);
        if (barH < 3) barH = 3; 
        int barY = chartBottom - barH;

        uint16_t barColor = (i == 11) ? COLOR_BAR_NOW : COLOR_BAR;
        
        tft.fillRoundRect(barX, barY, barW, barH, 2, barColor);
        if (barH > 4) {
             tft.fillRect(barX, chartBottom - 2, barW, 2, barColor);
        }

        if (i % 2 == 0 || i == 11) {
            int barCenter = barX + (barW / 2);
            
            for (int dy = yOffset; dy < chartBottom; dy += 5) {
                 if (tft.readPixel(barCenter, dy) == COLOR_BG) {
                     tft.drawPixel(barCenter, dy, COLOR_GRID);
                 }
            }

            int hoursAgo = 11 - i;
            tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);
            
            if (hoursAgo == 0) {
                tft.setTextColor(COLOR_BAR_NOW, COLOR_BG);
                tft.drawString("Teraz", barCenter, chartBottom + 5);
            } else if (hoursAgo > 1) { 
                tft.drawString("-" + String(hoursAgo) + "h", barCenter, chartBottom + 5);
            }
        }
    }

    // ==========================================
    // 7. DOLNY PANEL
    // ==========================================
    int widgetH = 30;
    int widgetY = tft.height() - widgetH - 8;
    int widgetX = 15; 
    int widgetW = tft.width() - 30;

    tft.fillRoundRect(widgetX, widgetY, widgetW, widgetH, 4, COLOR_WIDGET_BG);
    tft.drawRoundRect(widgetX, widgetY, widgetW, widgetH, 4, COLOR_WIDGET_BORDER);
    
    tft.setTextDatum(ML_DATUM); 
    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_WIDGET_BG);
    tft.setTextSize(1);
    
    // Prosta, czysta etykieta źródła danych
    tft.drawString(" OPEN-METEO API", widgetX + 10, widgetY + (widgetH / 2));

    // Wyświetlamy aktualną wartość ciśnienia
    tft.setTextDatum(MR_DATUM); 
    tft.setTextColor(COLOR_BAR_NOW, COLOR_WIDGET_BG);
    tft.setTextSize(2); 
    String currentPressureText = String(drawHistory[11], 1) + " hPa ";
    tft.drawString(currentPressureText, widgetX + widgetW - 5, widgetY + (widgetH / 2));
}