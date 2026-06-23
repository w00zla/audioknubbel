#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>
#include <stdint.h>

// Ein Theme = benanntes Set aus Band-Hintergründen + Tick-Ring-/Akzent-Styling.
// Die Tabelle (UI_THEMES) macht aus der früheren hartcodierten "if (CYBERPUNK)"-
// Logik Daten: ein neues Theme ist ein Tabelleneintrag + dessen LV_IMG_DECLAREs,
// kein Eingriff in ui.cpp. RGB wird als POD gehalten und am Use-Site via
// lv_color_make() gebaut (kein dynamischer Static-Init der lv_color_t).

struct UiRgb { uint8_t r, g, b; };

struct UiThemeDef {
    const char*                name;          // Switcher-Label
    const lv_img_dsc_t* const* backgrounds;   // [band_count]
    uint8_t                    band_count;
    UiRgb                      zone_low;      // Tick-Ring: unteres Drittel
    UiRgb                      zone_mid;      // mittleres Drittel
    UiRgb                      zone_high;     // oberes Drittel
    UiRgb                      dim;           // erloschene Ticks
    bool                       show_media_hints;
};

extern const UiThemeDef UI_THEMES[];
extern const int        UI_THEME_COUNT;

// Index modulo Theme-Anzahl (auch für negative Werte) — für Switcher-Scroll.
int ui_theme_wrap(int index);

#endif
