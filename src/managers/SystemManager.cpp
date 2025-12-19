#include "managers/SystemManager.h"

// Zmienne zewnętrzne (z main.cpp)
extern bool isImageDownloadInProgress; 
extern bool isWiFiConfigActive(); 

SystemManager::SystemManager() {
    lastCheckTime = 0;
    currentBrightness = 255; // Domyślna wartość startowa
}

void SystemManager::init() {
    // 1. Konfiguracja PWM dla ekranu
    ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
    
    restoreCorrectBrightness(); // Ustaw jasność startową
    Serial.println("💡 System Manager: PWM & Watchdog OK");

    // 2. Konfiguracja Watchdog Timer (WDT)
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);
}

void SystemManager::loop() {
    // 1. Nakarm psa (Watchdog)
    esp_task_wdt_reset();

    // 2. Zadania okresowe (np. co 1s)
    if (millis() - lastCheckTime > 1000) {
        lastCheckTime = millis();
        // Tutaj można dodać inne zadania w tle
    }
}

// Sprawdza czy jest czas na Deep Sleep (00:00 - 05:00)
bool SystemManager::isNightDeepSleepTime() {
    struct tm timeinfo;
    
    // Pobierz czas (0ms czekania, bo chcemy tylko sprawdzić to co mamy w pamięci)
    if (getLocalTime(&timeinfo, 0)) {
        
        // === ZABEZPIECZENIE 1970 ===
        // tm_year to lata od 1900 roku. 
        // Rok 2023 to 123 (2023 - 1900).
        // Jeśli rok jest mniejszy niż 2023, to znaczy, że NTP jeszcze nie zadziałało.
        if (timeinfo.tm_year < (2023 - 1900)) {
            Serial.println("⚠️ Czas niezsynchronizowany (Rok < 2023). Blokuję Deep Sleep.");
            return false; // NIE POZWÓL usnąć, dopóki nie pobierzesz aktualnej daty!
        }

        int hour = timeinfo.tm_hour;
        // Sprawdź przedział godzinowy (np. 00:00 - 05:00)
        if (hour >= HYBRID_SLEEP_START_HOUR && hour < HYBRID_SLEEP_END_HOUR) {
            return true;
        }
    }
    
    return false;
}

void SystemManager::setBrightness(uint8_t value) {
    currentBrightness = value; // Aktualizujemy zmienną śledzącą!
    ledcWrite(BACKLIGHT_PWM_CHANNEL, value);
}

void SystemManager::restoreCorrectBrightness() {
    struct tm timeinfo;
    uint8_t targetBrightness = BRIGHTNESS_DAY; // Domyślnie dzień

    if (getLocalTime(&timeinfo, 0)) {
        int hour = timeinfo.tm_hour;
        
        if (hour >= 8 && hour < 20) {
            targetBrightness = BRIGHTNESS_DAY;
        } else if (hour >= 20 && hour < 23) {
            targetBrightness = BRIGHTNESS_EVENING;
        } else {
            targetBrightness = BRIGHTNESS_NIGHT;
        }
    }
    
    // Ustawiamy jasność (bez fade, bo to przywracanie stanu)
    setBrightness(targetBrightness);
}

// NOWA FUNKCJA: Płynne ściemnianie/rozjaśnianie
void SystemManager::fadeBacklight(uint8_t from, uint8_t to) {
    if (from == to) return;

    int step = (from < to) ? 1 : -1; // Czy idziemy w górę czy w dół?
    int current = from;
    
    // Pętla zmiany jasności
    while (current != to) {
        current += step;
        ledcWrite(BACKLIGHT_PWM_CHANNEL, current);
        delay(2); // Szybkość efektu (2ms * 255 kroków = ~0.5 sekundy)
    }
    
    // Na koniec upewnij się, że wartość jest idealnie równa 'to' i zaktualizuj zmienną
    setBrightness(to);
}