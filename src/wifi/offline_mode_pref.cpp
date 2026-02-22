#include "wifi/offline_mode_pref.h"

#include <Preferences.h>

static const char* OFFLINE_PREF_NS = "settings";
static const char* OFFLINE_PREF_KEY = "offline";

bool loadOfflineModePref() {
  Preferences p;
  p.begin(OFFLINE_PREF_NS, false);
  const bool offline = p.getBool(OFFLINE_PREF_KEY, false);
  p.end();
  Serial.printf("[OFFLINE] loaded offline mode: %s\n", offline ? "true" : "false");
  return offline;
}

void saveOfflineModePref(bool offline) {
  Preferences p;
  p.begin(OFFLINE_PREF_NS, false);
  p.putBool(OFFLINE_PREF_KEY, offline);
  p.end();
  Serial.printf("[OFFLINE] saved offline mode: %s\n", offline ? "true" : "false");
}
