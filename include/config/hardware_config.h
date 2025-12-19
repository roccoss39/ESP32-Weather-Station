#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// === 1. PINY EKRANU (TFT ILI9341) ===
#ifndef TFT_BL
#define TFT_BL      25      // Pin podświetlenia
#endif

// === 2. PINY SENSORÓW ===
#define DHT22_PIN       4       // DHT22 Temperature/Humidity sensor
#define PIR_PIN         27      // Czujnik ruchu
#define LED_STATUS_PIN  2       // Wbudowana niebieska dioda

// === 3. KONFIGURACJA PWM (JASNOŚĆ) ===
#define BACKLIGHT_PWM_FREQ      5000    
#define BACKLIGHT_PWM_CHANNEL   0       
#define BACKLIGHT_PWM_RES       8       

// Poziomy jasności (0-255)
#define BRIGHTNESS_DAY          255     
#define BRIGHTNESS_EVENING      120     
#define BRIGHTNESS_NIGHT        10      

// === 4. KONFIGURACJA TRYBU HYBRYDOWEGO ===
#define HYBRID_SLEEP_START_HOUR 0   // Od 00:00 (Północ)
#define HYBRID_SLEEP_END_HOUR   5   // Do 05:00 (Deep Sleep)

// Timery
#define SCREEN_AUTO_OFF_MS      10000   // 10 sekund do wygaszenia
#define PIR_DEBOUNCE_TIME       500     
#define LED_FLASH_DURATION      200     
#define CONFIG_MODE_TIMEOUT_MS  600000  
#define WDT_TIMEOUT_SECONDS     15      

#define USE_HYBRID_SLEEP  1

#endif