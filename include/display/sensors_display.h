#ifndef SENSORS_DISPLAY_H
#define SENSORS_DISPLAY_H

#include <TFT_eSPI.h>

void displayLocalSensors(TFT_eSPI& tft, bool onlyUpdate = false);

#endif