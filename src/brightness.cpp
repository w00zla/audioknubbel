#include "brightness.h"
#ifdef ARDUINO
#include <Preferences.h>
#include "display.h"

static Preferences s_prefs;
static int         s_brightness = 100;

void brightnessInit() {
    s_prefs.begin("audioknubbel", false);            // NVS-Namespace, read/write
    s_brightness = s_prefs.getInt("bright", 100); // Default 100 % auf frischem Board
    if (s_brightness < 5)   s_brightness = 5;
    if (s_brightness > 100) s_brightness = 100;
    displayBacklightSetLevel(brightnessPctToDuty(s_brightness));
}

void brightnessSet(int pct) {
    if (pct < 5)   pct = 5;
    if (pct > 100) pct = 100;
    s_brightness = pct;
    displayBacklightSetLevel(brightnessPctToDuty(pct));
    s_prefs.putInt("bright", pct);
}

int brightnessGet() { return s_brightness; }
#endif
