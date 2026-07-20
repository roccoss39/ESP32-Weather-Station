#include "display/display_pressure.h"
#include "display_utils.h"
#include "weather/weather_data.h"
#include "weather/open_meteo_api.h"
#include <Preferences.h> // <-- DODANO BILIOTEKĘ PAMIĘCI

// Definicja globalnej flagi wyboru wysokości ciśnienia
bool showPressureAtSeaLevel = true; 

// Funkcja zachowana dla wstecznej kompatybilności.
void savePressureToHistory(float currentPressure) {
    // Pusta - cała historia pobierana jest bezpośrednio z Open-Meteo API
}

void displayPressureScreen(TFT_eSPI& tft) {
    // Kolory interfejsu (Format RGB565)
    uint16_t COLOR_BG = TFT_BLACK;
    uint16_t COLOR_GRID = tft.color565(50, 50, 50);
    uint16_t COLOR_TEXT_MUTED = tft.color565(150, 150, 150);
    uint16_t COLOR_BAR = tft.color565(80, 130, 180);
    uint16_t COLOR_BAR_NOW = tft.color565(0, 200, 255);
    uint16_t COLOR_WIDGET_BG = tft.color565(20, 25, 30);
    uint16_t COLOR_WIDGET_BORDER = tft.color565(80, 80, 80);

    // ==========================================
    // NOWOŚĆ: ODCZYT STANU Z PAMIĘCI FLASH
    // (Zapewnia pamięć wyboru po resecie i wybudzeniu)
    // ==========================================
    Preferences prefs;
    prefs.begin("settings", true); // true = tryb tylko do odczytu
    // Jeśli to pierwsze uruchomienie, domyślnie zwróci true (MSL)
    showPressureAtSeaLevel = prefs.getBool("msl_mode", true); 
    prefs.end();

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
        clearAndShowMessage(tft, "Oczekiwanie na\ndane z internetu...", TFT_WHITE, 2);
        return;
    }

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
    
    int barSpacing = 4; 
    int barW = (chartW / maxSlots) - barSpacing;

    // ==========================================
    // 5. RYSOWANIE SIATKI (GRID) - BŁYSKAWICZNE
    // ==========================================
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM); 
    tft.setTextColor(COLOR_TEXT_MUTED, COLOR_BG);

    // Linie poziome
    for (int i = 0; i <= 3; i++) {
        float val = maxP - i * ((maxP - minP) / 3.0);
        int y = yOffset + i * (chartH / 3);
        
        for (int dx = xOffset; dx < xOffset + chartW; dx += 5) {
            tft.drawPixel(dx, y, COLOR_GRID);
        }
        tft.drawString(String(val, 1), xOffset - 5, y);
    }

    // Linie pionowe - RYSOWANE ZANIM NARYSUJEMY SŁUPKI!
    for (int i = 0; i < 12; i++) {
        if (i % 2 == 0 || i == 11) {
            int barX = xOffset + (i * (chartW / maxSlots)) + (barSpacing / 2);
            int barCenter = barX + (barW / 2);
            
            for (int dy = yOffset; dy < chartBottom; dy += 5) {
                 tft.drawPixel(barCenter, dy, COLOR_GRID);
            }
        }
    }

    // ==========================================
    // 6. RYSOWANIE SŁUPKÓW I OSI X
    // ==========================================
    tft.setTextDatum(TC_DATUM); 

    for (int i = 0; i < 12; i++) {
        int barX = xOffset + (i * (chartW / maxSlots)) + (barSpacing / 2);
        
        float normalizedValue = (drawHistory[i] - minP) / (maxP - minP); 
        int barH = (int)(normalizedValue * chartH);
        if (barH < 3) barH = 3; 
        int barY = chartBottom - barH;

        uint16_t barColor = (i == 11) ? COLOR_BAR_NOW : COLOR_BAR;
        
        // Rysujemy słupek (naturalnie przykryje pionowe kropki siatki pod spodem!)
        tft.fillRoundRect(barX, barY, barW, barH, 2, barColor);
        if (barH > 4) {
             tft.fillRect(barX, chartBottom - 2, barW, 2, barColor);
        }

        // Napisy na osi X
        if (i % 2 == 0 || i == 11) {
            int barCenter = barX + (barW / 2);
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
    // 7. DOLNY PANEL: INTERAKTYWNY PRZYCISK OPCOW
    // ==========================================
    int widgetH = 30;
    int widgetY = tft.height() - widgetH - 8;
    int widgetX = 15; 
    int widgetW = tft.width() - 30;

    // Przycisk przełącznika wysokości (szerokość 170px)
    int btnW = 170;
    int btnH = widgetH;
    int btnX = widgetX;
    int btnY = widgetY;

    // Kolory przycisku dopasowują się do stanu aktywności
    uint16_t btnBg = showPressureAtSeaLevel ? tft.color565(20, 40, 70) : tft.color565(35, 35, 40);
    uint16_t btnBorder = showPressureAtSeaLevel ? COLOR_BAR_NOW : COLOR_WIDGET_BORDER;

    tft.fillRoundRect(btnX, btnY, btnW, btnH, 4, btnBg);
    tft.drawRoundRect(btnX, btnY, btnW, btnH, 4, btnBorder);
    
    tft.setTextDatum(MC_DATUM); 
    tft.setTextColor(showPressureAtSeaLevel ? COLOR_BAR_NOW : TFT_WHITE, btnBg);
    tft.setTextSize(1);
    
    if (showPressureAtSeaLevel) {
        tft.drawString("Poz. morza (MSL) [X]", btnX + (btnW / 2), btnY + (btnH / 2));
    } else {
        tft.drawString("Poz. gruntu (Stacja)", btnX + (btnW / 2), btnY + (btnH / 2));
    }

    // Panel odczytu wartości (szerokość 110px z 10px przerwy)
    int valX = btnX + btnW + 10;
    int valW = widgetW - btnW - 10;
    int valY = widgetY;

    tft.fillRoundRect(valX, valY, valW, widgetH, 4, COLOR_WIDGET_BG);
    tft.drawRoundRect(valX, valY, valW, widgetH, 4, COLOR_WIDGET_BORDER);

    tft.setTextDatum(MR_DATUM); 
    tft.setTextColor(COLOR_BAR_NOW, COLOR_WIDGET_BG);
    tft.setTextSize(2); 
    String currentPressureText = String(drawHistory[11], 1) + " ";
    tft.drawString(currentPressureText, valX + valW - 22, valY + (widgetH / 2));

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_WIDGET_BG);
    tft.drawString("hPa", valX + valW - 5, valY + (widgetH / 2));
}