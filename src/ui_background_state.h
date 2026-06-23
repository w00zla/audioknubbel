#ifndef UI_BACKGROUND_STATE_H
#define UI_BACKGROUND_STATE_H

enum UiBackgroundBand {
    UI_BG_00 = 0,
    UI_BG_01 = 1,
    UI_BG_02 = 2,
    UI_BG_03 = 3,
    UI_BG_04 = 4,
    UI_BG_05 = 5,
    UI_BG_06 = 6,
    UI_BG_07 = 7,
    UI_BG_08 = 8,
    UI_BG_09 = 9,
    UI_BG_10 = 10,
    UI_BG_11 = 11,
    UI_BG_12 = 12,
    UI_BG_13 = 13,
    UI_BG_14 = 14
};

constexpr int ui_background_state_count() {
    return 15;
}

constexpr int ui_background_nominal_band_for_volume(int pct) {
    int clamped = pct < 0 ? 0 : (pct > 100 ? 100 : pct);

    if (clamped == 0) return UI_BG_00;
    return 1 + ((clamped - 1) * (ui_background_state_count() - 1)) / 100;
}

constexpr int ui_background_lower_edge(int band) {
    if (band <= UI_BG_00) return 0;
    for (int pct = 1; pct <= 100; ++pct) {
        if (ui_background_nominal_band_for_volume(pct) == band) return pct;
    }
    return 100;
}

constexpr int ui_background_upper_edge(int band) {
    if (band <= UI_BG_00) return 0;
    for (int pct = 100; pct >= 1; --pct) {
        if (ui_background_nominal_band_for_volume(pct) == band) return pct;
    }
    return 100;
}

constexpr UiBackgroundBand ui_background_band_for_volume(UiBackgroundBand current, int pct) {
    int clamped = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    int nominal = ui_background_nominal_band_for_volume(clamped);
    int currentIndex = (int)current;

    if (nominal == currentIndex) return current;
    if (currentIndex < UI_BG_00 || currentIndex >= ui_background_state_count()) return (UiBackgroundBand)nominal;
    if (clamped == 0 || currentIndex == UI_BG_00) return (UiBackgroundBand)nominal;

    int hysteresis = 2;

    if (nominal > currentIndex && clamped <= ui_background_upper_edge(currentIndex) + hysteresis) {
        return current;
    }
    if (nominal < currentIndex && clamped >= ui_background_lower_edge(currentIndex) - hysteresis) {
        return current;
    }

    return (UiBackgroundBand)nominal;
}

static_assert(ui_background_nominal_band_for_volume(-8) == UI_BG_00, "volume clamps below 0");
static_assert(ui_background_state_count() == 15, "15 background states are available");
static_assert(ui_background_nominal_band_for_volume(0) == UI_BG_00, "0% uses zero background");
static_assert(ui_background_nominal_band_for_volume(1) == UI_BG_01, "1% starts idle");
static_assert(ui_background_nominal_band_for_volume(8) == UI_BG_01, "8% stays idle");
static_assert(ui_background_nominal_band_for_volume(9) == UI_BG_02, "9% enters the next background");
static_assert(ui_background_nominal_band_for_volume(93) == UI_BG_13, "93% is near peak");
static_assert(ui_background_nominal_band_for_volume(100) == UI_BG_14, "100% uses peak background");
static_assert(ui_background_nominal_band_for_volume(120) == UI_BG_14, "volume clamps above 100");
static_assert(ui_background_band_for_volume(UI_BG_03, 23) == UI_BG_03, "hysteresis holds upward edge");
static_assert(ui_background_band_for_volume(UI_BG_03, 25) == UI_BG_04, "hysteresis releases upward edge");
static_assert(ui_background_band_for_volume(UI_BG_04, 22) == UI_BG_04, "hysteresis holds downward edge");
static_assert(ui_background_band_for_volume(UI_BG_04, 20) == UI_BG_03, "hysteresis releases downward edge");

#endif
