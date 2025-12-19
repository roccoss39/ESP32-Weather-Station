#include "managers/SystemManager.h"

SystemManager::SystemManager() {
    lastCheckTime = 0;
}

void SystemManager::init() {
    // 1. Konfiguracja PWM
    ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
    restoreCorrectBrightness(); 
    Serial.println("💡 System Manager: PWM Init OK");

    // 2. Watchdog
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);
}

void SystemManager::loop() {
    esp_task_wdt_reset(); // Karm psa
}

bool SystemManager::isNightDeepSleepTime() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        int hour = timeinfo.tm_hour;
        if (hour >= HYBRID_SLEEP_START_HOUR && hour < HYBRID_SLEEP_END_HOUR) {
            return true;
        }
    }
    return false;
}

void SystemManager::setBrightness(uint8_t value) {
    ledcWrite(BACKLIGHT_PWM_CHANNEL, value);
}

void SystemManager::restoreCorrectBrightness() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        int hour = timeinfo.tm_hour;
        if (hour >= 8 && hour < 20) ledcWrite(BACKLIGHT_PWM_CHANNEL, BRIGHTNESS_DAY);
        else if (hour >= 20 && hour < 23) ledcWrite(BACKLIGHT_PWM_CHANNEL, BRIGHTNESS_EVENING);
        else ledcWrite(BACKLIGHT_PWM_CHANNEL, BRIGHTNESS_NIGHT);
    } else {
        ledcWrite(BACKLIGHT_PWM_CHANNEL, BRIGHTNESS_DAY);
    }
}