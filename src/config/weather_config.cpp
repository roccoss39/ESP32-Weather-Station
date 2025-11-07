#include "config/weather_config.h"
#include "config/secrets.h"

// --- DEFINICJE ZMIENNYCH KONFIGURACYJNYCH ---
// UWAGA: API credentials przeniesione do secrets.h dla bezpieczeństwa

// --- USTAWIENIA CZASOWE ---
const unsigned long WEATHER_UPDATE_INTERVAL = 600000; // 10 minut w ms