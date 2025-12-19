#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <time.h>
#include "config/hardware_config.h"

class SystemManager {
private:
    unsigned long lastCheckTime = 0;

public:
    SystemManager();
    
    void init();
    void loop();
    
    // Sterowanie jasnością
    void setBrightness(uint8_t value);
    void restoreCorrectBrightness(); 
    
    // Sprawdza czy jest czas na Deep Sleep
    bool isNightDeepSleepTime(); 
};

#endif