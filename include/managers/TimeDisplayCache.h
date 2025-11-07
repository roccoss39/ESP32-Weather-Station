#ifndef TIME_DISPLAY_CACHE_H
#define TIME_DISPLAY_CACHE_H

#include <Arduino.h>
#include <WiFi.h>

/**
 * ⏰ TimeDisplayCache - Smart cache management dla time display
 * 
 * Zastępuje 4 extern variables lepszą enkapsulacją:
 * - char timeStrPrev[9]
 * - char dateStrPrev[11] 
 * - String dayStrPrev
 * - int wifiStatusPrev
 */
class TimeDisplayCache {
private:
    // Previous values for comparison
    char prevTimeStr[9];
    char prevDateStr[11];
    String prevDayStr;
    int prevWifiStatus;

public:
    // --- CONSTRUCTOR ---
    TimeDisplayCache() {
        resetCache();
    }
    
    // --- GETTERS ---
    const char* getPrevTimeStr() const { return prevTimeStr; }
    const char* getPrevDateStr() const { return prevDateStr; }
    String getPrevDayStr() const { return prevDayStr; }
    int getPrevWifiStatus() const { return prevWifiStatus; }
    
    // --- SETTERS ---
    void setPrevTimeStr(const char* timeStr) { 
        strncpy(prevTimeStr, timeStr, sizeof(prevTimeStr) - 1); 
        prevTimeStr[sizeof(prevTimeStr) - 1] = '\0'; // Ensure null termination
    }
    
    void setPrevDateStr(const char* dateStr) { 
        strncpy(prevDateStr, dateStr, sizeof(prevDateStr) - 1); 
        prevDateStr[sizeof(prevDateStr) - 1] = '\0'; // Ensure null termination
    }
    
    void setPrevDayStr(const String& dayStr) { 
        prevDayStr = dayStr; 
    }
    
    void setPrevWifiStatus(int status) { 
        prevWifiStatus = status; 
    }
    
    // --- UTILITY METHODS ---
    
    /**
     * Resetuje cały cache - wymusza ponowne rysowanie
     * Użyj przy przełączaniu ekranów
     */
    void resetCache() {
        strcpy(prevTimeStr, "");
        strcpy(prevDateStr, "");
        prevDayStr = "";
        prevWifiStatus = -1;
    }
    
    /**
     * Sprawdza czy czas się zmienił
     * @param currentTime Aktualny czas jako string
     * @return true jeśli czas się zmienił
     */
    bool hasTimeChanged(const char* currentTime) const {
        return strcmp(prevTimeStr, currentTime) != 0;
    }
    
    /**
     * Sprawdza czy data się zmieniła
     * @param currentDate Aktualna data jako string
     * @return true jeśli data się zmieniła
     */
    bool hasDateChanged(const char* currentDate) const {
        return strcmp(prevDateStr, currentDate) != 0;
    }
    
    /**
     * Sprawdza czy dzień się zmienił
     * @param currentDay Aktualny dzień jako String
     * @return true jeśli dzień się zmienił
     */
    bool hasDayChanged(const String& currentDay) const {
        return prevDayStr != currentDay;
    }
    
    /**
     * Sprawdza czy status WiFi się zmienił
     * @param currentStatus Aktualny status WiFi
     * @return true jeśli status się zmienił
     */
    bool hasWifiStatusChanged(int currentStatus) const {
        return prevWifiStatus != currentStatus;
    }
    
    /**
     * Sprawdza czy COKOLWIEK się zmieniło
     * @param currentTime Aktualny czas
     * @param currentDate Aktualna data  
     * @param currentDay Aktualny dzień
     * @param currentWifiStatus Aktualny status WiFi
     * @return true jeśli coś się zmieniło
     */
    bool hasAnyChanged(const char* currentTime, const char* currentDate, 
                      const String& currentDay, int currentWifiStatus) const {
        return hasTimeChanged(currentTime) || 
               hasDateChanged(currentDate) || 
               hasDayChanged(currentDay) || 
               hasWifiStatusChanged(currentWifiStatus);
    }
    
    /**
     * Aktualizuje cache nowymi wartościami
     * @param currentTime Aktualny czas
     * @param currentDate Aktualna data
     * @param currentDay Aktualny dzień  
     * @param currentWifiStatus Aktualny status WiFi
     */
    void updateCache(const char* currentTime, const char* currentDate,
                    const String& currentDay, int currentWifiStatus) {
        setPrevTimeStr(currentTime);
        setPrevDateStr(currentDate);
        setPrevDayStr(currentDay);
        setPrevWifiStatus(currentWifiStatus);
    }
    
    /**
     * Smart update - aktualizuje cache tylko jeśli się zmienił
     * @param currentTime Aktualny czas
     * @param currentDate Aktualna data
     * @param currentDay Aktualny dzień
     * @param currentWifiStatus Aktualny status WiFi
     * @return true jeśli cache został zaktualizowany
     */
    bool smartUpdate(const char* currentTime, const char* currentDate,
                    const String& currentDay, int currentWifiStatus) {
        if (hasAnyChanged(currentTime, currentDate, currentDay, currentWifiStatus)) {
            updateCache(currentTime, currentDate, currentDay, currentWifiStatus);
            return true;
        }
        return false;
    }
    
    // --- DEBUG ---
    void printDebugInfo() const {
        Serial.println("=== TimeDisplayCache Debug ===");
        Serial.println("Time: '" + String(prevTimeStr) + "'");
        Serial.println("Date: '" + String(prevDateStr) + "'");
        Serial.println("Day: '" + prevDayStr + "'");
        Serial.println("WiFi Status: " + String(prevWifiStatus));
    }
};

#endif