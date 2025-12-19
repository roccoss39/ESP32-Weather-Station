#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/**
 * 🔌 CENTRALNA KONFIGURACJA SPRZĘTOWA
 * * Tutaj definiujemy fizyczne podłączenia (PINY), ustawienia PWM
 * oraz tryby pracy sprzętu.
 * * UWAGA: Wszystkie czasy (timeouty, delaye) znajdują się w timing_config.h
 */

 // UPDATE:
 #define FIRMWARE_VERSION  1.0  // Zmieniaj to przy każdej nowej wersji!
 #define GITHUB_FIRMWARE_URL "https://raw.githubusercontent.com/TwojUser/TwojeRepo/main/firmware.bin"

// === 1. PINY EKRANU (TFT ILI9341) ===
// Definicje zgodne z biblioteką TFT_eSPI
#ifndef TFT_MISO
#define TFT_MISO    19
#endif
#ifndef TFT_MOSI
#define TFT_MOSI    23
#endif
#ifndef TFT_SCLK
#define TFT_SCLK    18
#endif
#ifndef TFT_CS
#define TFT_CS      5
#endif
#ifndef TFT_DC
#define TFT_DC      15
#endif
#ifndef TFT_RST
#define TFT_RST     -1  // Reset podłączony do RST ESP32 lub -1
#endif
#ifndef TOUCH_CS
#define TOUCH_CS    22
#endif

// Pin podświetlenia (sterowany przez PWM w SystemManager)
#define TFT_BL      25      

// === 2. PINY SENSORÓW I URZĄDZEŃ ===
#define DHT22_PIN       4       // Czujnik temperatury/wilgotności
#define PIR_PIN         27      // Czujnik ruchu (HC-SR501 / MOD-01655)
#define LED_STATUS_PIN  2       // Wbudowana niebieska dioda ESP32

// Wymiary ekranu
#ifndef TFT_WIDTH
#define TFT_WIDTH   320
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT  240
#endif

// === 3. KONFIGURACJA PWM (JASNOŚĆ EKRANU) ===
#define BACKLIGHT_PWM_FREQ      5000    // 5 kHz
#define BACKLIGHT_PWM_CHANNEL   0       // Kanał 0
#define BACKLIGHT_PWM_RES       8       // 8-bit rozdzielczość (0-255)

// Poziomy jasności dla SystemManagera
#define BRIGHTNESS_DAY          255     // 100% mocy
#define BRIGHTNESS_EVENING      120     // ~50% mocy
#define BRIGHTNESS_NIGHT        10      // Minimalne podświetlenie (gdy nie śpi)

// === 4. LOGIKA OSZCZĘDZANIA ENERGII ===
// Ustaw 1: TRYB HYBRYDOWY (Dzień: Ekran OFF + CPU ON, Noc: Deep Sleep)
// Ustaw 0: PEŁNE UŚPIENIE (Zawsze Deep Sleep po wygaszeniu - resetuje stację)
#define USE_HYBRID_SLEEP        1  

// Godziny dla trybu hybrydowego (działają tylko gdy USE_HYBRID_SLEEP == 1)
#define HYBRID_SLEEP_START_HOUR 0   // Od 00:00 (Północ) -> Deep Sleep
#define HYBRID_SLEEP_END_HOUR   5   // Do 05:00 -> Deep Sleep

// === 5. KONFIGURACJA SPI ===
#ifndef SPI_FREQUENCY
    #define SPI_FREQUENCY   40000000    // 40 MHz
#endif
#ifndef SPI_READ_FREQUENCY
    #define SPI_READ_FREQUENCY 20000000 // 20 MHz
#endif
#ifndef SPI_TOUCH_FREQUENCY
    #define SPI_TOUCH_FREQUENCY 2500000 // 2.5 MHz
#endif

#endif // HARDWARE_CONFIG_H