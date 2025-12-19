#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <time.h>
#include "config/hardware_config.h"

class SystemManager {
private:
    unsigned long lastCheckTime = 0;
    uint8_t currentBrightness = 255;  // Track current brightness

public:
    SystemManager();
    
    void init();
    void loop();
    
    // Sterowanie jasnością
    void setBrightness(uint8_t value);
    uint8_t getCurrentBrightness() const { return currentBrightness; }  // Getter
    void restoreCorrectBrightness(); 
    void fadeBacklight(uint8_t from, uint8_t to);
    
    // Sprawdza czy jest czas na Deep Sleep
    bool isNightDeepSleepTime(); 
};

#endif