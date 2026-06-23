#include "ui_theme.h"
#include "ui_backgrounds.h"

// Band-Hintergründe pro Theme. Reihenfolge = UiBackgroundBand (UI_BG_00..14).
static const lv_img_dsc_t* const PLASMA_BG[] = {
    &ui_bg_00, &ui_bg_01, &ui_bg_02, &ui_bg_03, &ui_bg_04,
    &ui_bg_05, &ui_bg_06, &ui_bg_07, &ui_bg_08, &ui_bg_09,
    &ui_bg_10, &ui_bg_11, &ui_bg_12, &ui_bg_13, &ui_bg_14,
};

static const lv_img_dsc_t* const CYBERPUNK_BG[] = {
    &ui_bg_cp_00, &ui_bg_cp_01, &ui_bg_cp_02, &ui_bg_cp_03, &ui_bg_cp_04,
    &ui_bg_cp_05, &ui_bg_cp_06, &ui_bg_cp_07, &ui_bg_cp_08, &ui_bg_cp_09,
    &ui_bg_cp_10, &ui_bg_cp_11, &ui_bg_cp_12, &ui_bg_cp_13, &ui_bg_cp_14,
};

static const lv_img_dsc_t* const BIOS_CRT_BG[] = {
    &ui_bg_bios_00, &ui_bg_bios_01, &ui_bg_bios_02, &ui_bg_bios_03, &ui_bg_bios_04,
    &ui_bg_bios_05, &ui_bg_bios_06, &ui_bg_bios_07, &ui_bg_bios_08, &ui_bg_bios_09,
    &ui_bg_bios_10, &ui_bg_bios_11, &ui_bg_bios_12, &ui_bg_bios_13, &ui_bg_bios_14,
};

static const lv_img_dsc_t* const DEEP_SPACE_BG[] = {
    &ui_bg_space_00, &ui_bg_space_01, &ui_bg_space_02, &ui_bg_space_03, &ui_bg_space_04,
    &ui_bg_space_05, &ui_bg_space_06, &ui_bg_space_07, &ui_bg_space_08, &ui_bg_space_09,
    &ui_bg_space_10, &ui_bg_space_11, &ui_bg_space_12, &ui_bg_space_13, &ui_bg_space_14,
};

static const lv_img_dsc_t* const LAVA_CORE_BG[] = {
    &ui_bg_lava_00, &ui_bg_lava_01, &ui_bg_lava_02, &ui_bg_lava_03, &ui_bg_lava_04,
    &ui_bg_lava_05, &ui_bg_lava_06, &ui_bg_lava_07, &ui_bg_lava_08, &ui_bg_lava_09,
    &ui_bg_lava_10, &ui_bg_lava_11, &ui_bg_lava_12, &ui_bg_lava_13, &ui_bg_lava_14,
};

static const lv_img_dsc_t* const ICE_NEON_BG[] = {
    &ui_bg_ice_00, &ui_bg_ice_01, &ui_bg_ice_02, &ui_bg_ice_03, &ui_bg_ice_04,
    &ui_bg_ice_05, &ui_bg_ice_06, &ui_bg_ice_07, &ui_bg_ice_08, &ui_bg_ice_09,
    &ui_bg_ice_10, &ui_bg_ice_11, &ui_bg_ice_12, &ui_bg_ice_13, &ui_bg_ice_14,
};

// Farben/Flags 1:1 aus der bisherigen ui.cpp-Logik übernommen.
const UiThemeDef UI_THEMES[] = {
    { "Plasma",    PLASMA_BG,    15,
      {  0, 220,  90}, {255, 170,   0}, {255,  50,  50}, { 45, 45, 45}, true  },
    { "Cyberpunk", CYBERPUNK_BG, 15,
      {232, 251, 255}, {158, 235, 255}, {255,  90, 122}, { 34, 38, 50}, false },
    { "BIOS CRT",  BIOS_CRT_BG,  15,
      { 96, 255,  48}, {196, 255,  32}, {255, 176,  24}, { 18, 42, 16}, false },
    { "Deep Space", DEEP_SPACE_BG, 15,
      { 80, 172, 255}, {178, 112, 255}, {255, 150, 240}, { 24, 28, 48}, false },
    { "Lava Core", LAVA_CORE_BG, 15,
      {255,  84,  20}, {255, 156,  24}, {255, 232,  96}, { 48, 22, 16}, false },
    { "Ice Neon",  ICE_NEON_BG,  15,
      { 90, 230, 255}, {170, 245, 255}, {255, 128, 255}, { 22, 30, 48}, false },
};

const int UI_THEME_COUNT = (int)(sizeof(UI_THEMES) / sizeof(UI_THEMES[0]));

int ui_theme_wrap(int index) {
    int n = UI_THEME_COUNT;
    if (n <= 0) return 0;
    index %= n;
    if (index < 0) index += n;
    return index;
}
