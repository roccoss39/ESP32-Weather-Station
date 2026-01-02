#ifndef SENSORS_DISPLAY_H
#define SENSORS_DISPLAY_H

#include <TFT_eSPI.h>

/**
 * Wyświetla ekran z lokalnymi sensorami (DHT22) i statusem systemu.
 * Obsługuje tryb kompaktowy (online) i pełny (offline).
 * * @param tft Referencja do obiektu wyświetlacza
 */
void displayLocalSensors(TFT_eSPI& tft);

#endif // SENSORS_DISPLAY_H