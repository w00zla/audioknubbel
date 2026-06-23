#include "standby_cfg.h"
#ifdef ARDUINO
#include <Preferences.h>

static Preferences s_prefs;
static int         s_standby = 15;

void standbyCfgInit() {
    s_prefs.begin("audioknubbel", false);             // NVS-Namespace, read/write
    s_standby = s_prefs.getInt("standby", 15);    // Default 15 s
    if (s_standby < 5)  s_standby = 5;
    if (s_standby > 60) s_standby = 60;
}

void standbyCfgSet(int sec) {
    if (sec < 5)  sec = 5;
    if (sec > 60) sec = 60;
    s_standby = sec;
    s_prefs.putInt("standby", sec);
}

int standbyCfgGet() { return s_standby; }
#endif
