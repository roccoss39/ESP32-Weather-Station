#ifndef TIMING_CONFIG_H
#define TIMING_CONFIG_H

// === MOTION & POWER TIMEOUTS ===
#define SCREEN_AUTO_OFF_MS      3000   // 3s - wygaszanie ekranu
#define PIR_DEBOUNCE_TIME       500     // 500ms - stabilizacja PIR
#define LED_FLASH_DURATION      200     // 200ms - mrugnięcie diodą
#define MOTION_CONFIG_TIMEOUT   600000  // 10 min - timeout w menu config
#define WDT_TIMEOUT_SECONDS     35     // 35s - Watchdog
#define OFFLINE_MODE_TIMEOUT    30000
#define SCREEN_SWITCH_INTERVAL_ 3000

// === WIFI TOUCH INTERFACE ===
#define WIFI_LOSS_TIMEOUT           60000   
#define WIFI_CONFIG_MODE_TIMEOUT    180000  
#define WIFI_LONG_PRESS_TIME        3000    
#define WIFI_RECONNECT_INTERVAL     9000   
#define WIFI_CONNECTION_TIMEOUT     10000   
#define WIFI_STATUS_CHECK_INTERVAL  2000    
#define WIFI_FINAL_WAIT_TIME        3000    
#define WIFI_UI_UPDATE_INTERVAL     5000    
#define WIFI_PROGRESS_START_TIME    1000    
#define CONFIG_MODE_TIMEOUT_MS 18000

// === DELAY TIMES ===
#define DELAY_STABILIZATION         1000    
#define DELAY_CONNECTION_STEP       500      
#define DELAY_ERROR_DISPLAY         2000    
#define DELAY_SUCCESS_DISPLAY       3000    
#define DELAY_VISUAL_FEEDBACK       500     
#define DELAY_WAKE_UP_MESSAGE       2000    
#define DELAY_SLEEP_MESSAGE         3000    

// === WEATHER API INTERVALS ===
#define WEATHER_UPDATE_NORMAL       600000  
#define WEATHER_UPDATE_ERROR        10000   
#define WEATHER_FORCE_REFRESH       10000   
#define WEEKLY_UPDATE_INTERVAL     14400000  
#define WEEKLY_UPDATE_ERROR         5000    

// === DISPLAY REFRESH ===
#define DISPLAY_UPDATE_INTERVAL     1000    
#define COUNTDOWN_UPDATE_INTERVAL   1000    

#endif