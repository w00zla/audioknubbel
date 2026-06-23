#include "theme_cfg.h"
#ifdef ARDUINO
#include <Preferences.h>

static Preferences s_prefs;
static int         s_theme = 0;

void themeCfgInit() {
    s_prefs.begin("audioknubbel", false);          // gleicher NVS-Namespace wie brightness/standby
    s_theme = s_prefs.getInt("theme", 0);      // Default 0 (erstes Theme)
    if (s_theme < 0) s_theme = 0;
}

void themeCfgSet(int index) {
    if (index < 0) index = 0;
    s_theme = index;
    s_prefs.putInt("theme", index);
}

int themeCfgGet() { return s_theme; }
#endif
