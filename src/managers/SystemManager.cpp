#include "managers/SystemManager.h"
#include "timing_config.h"
#include "weather/weather_data.h"

extern bool isImageDownloadInProgress; 
extern bool isWiFiConfigActive(); 

SystemManager::SystemManager() {
    lastCheckTime = 0;
    currentBrightness = 255; 
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

    // 2. Zadania okresowe 
    if (millis() - lastCheckTime > 1000) {
        lastCheckTime = millis();
        
    }
}

// Sprawdza czy jest czas na Deep Sleep (00:00 - 05:00)
bool SystemManager::isNightDeepSleepTime() {
    struct tm timeinfo;
    
    // Pobierz czas (0ms czekania, bo sprawdzamy tylko stan pamięci)
    if (getLocalTime(&timeinfo, 0)) {
        
        // === ZABEZPIECZENIE 1970 ===
        // Jeśli rok jest mniejszy niż 2023 (czyli 123 lata od 1900),
        // to znaczy, że NTP jeszcze nie zadziałało po resecie.
        // Wtedy NIE WOLNO usypiać, bo wpadniemy w pętlę restartów.
        if (timeinfo.tm_year < (2023 - 1900)) {
            Serial.println("⚠️ Czas niezsynchronizowany (Rok < 2023). Blokuję Deep Sleep.");
            return false; 
        }

        int hour = timeinfo.tm_hour;
        if (hour >= HYBRID_SLEEP_START_HOUR && hour < HYBRID_SLEEP_END_HOUR) {
            return true;
        }
    }
    // Jeśli w ogóle nie ma czasu, też nie ryzykuj usypiania
    return false;
}

void SystemManager::setBrightness(uint8_t value) {
    currentBrightness = value;
    
    if (value == 0) {
        // Twarde wyłączenie - odłącz PWM i ustaw stan niski
        ledcDetachPin(TFT_BL);
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, LOW); // Całkowite odcięcie prądu
    } else {
        // Jeśli wcześniej było 0, musimy ponownie podłączyć PWM
        if (ledcRead(BACKLIGHT_PWM_CHANNEL) == 0 && value > 0) {
             ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
        }
        // Upewnij się, że PWM jest podpięty (dla bezpieczeństwa przy każdym wywołaniu > 0)
        ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
        ledcWrite(BACKLIGHT_PWM_CHANNEL, value);
    }
}

void SystemManager::restoreCorrectBrightness() {
    uint8_t targetBrightness = BRIGHTNESS_DAY; // Domyślnie dzień
    unsigned long currentUnixTime = time(nullptr); // Pobierz aktualny czas w formacie UNIX

    // 1. GŁÓWNA LOGIKA: Oparta na Słońcu (jeśli mamy dane z internetu)
    if (weather.isValid && weather.sunrise > 0 && weather.sunset > 0) {
        
        if (currentUnixTime >= weather.sunrise && currentUnixTime < weather.sunset) {
            // Słońce na niebie -> Pełna jasność
            targetBrightness = BRIGHTNESS_DAY;
        } 
        else if (currentUnixTime >= weather.sunset && currentUnixTime < (weather.sunset + 5400)) {
            // Od zachodu słońca przez kolejne 1.5 godziny (5400 sekund) -> Wieczór
            targetBrightness = BRIGHTNESS_EVENING;
        } 
        else {
            // Przed wschodem lub długo po zachodzie -> Noc
            targetBrightness = BRIGHTNESS_NIGHT;
        }
        
    } 
    // 2. AWARYJNA LOGIKA (FALLBACK): Jeśli tryb Offline lub API pogody nie odpowiedziało
    else {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) {
            int hour = timeinfo.tm_hour;
            
            if (hour >= 6 && hour < 20) {
                targetBrightness = BRIGHTNESS_DAY;
            } else if (hour >= 20 && hour < 23) {
                targetBrightness = BRIGHTNESS_EVENING;
            } else {
                targetBrightness = BRIGHTNESS_NIGHT;
            }
        }
    }
    
    // Ustawiamy jasność (bez fade, bo to przywracanie stanu)
    setBrightness(targetBrightness);
}

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
    
    // Na koniec upewnienie się, że wartość jest idealnie równa 'to' i zaktualizowanie zmiennej
    setBrightness(to);
}