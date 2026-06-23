#pragma once
#include <stdbool.h>
#include "ui_theme.h"

enum UiMediaFeedback {
    UI_MEDIA_PREV,
    UI_MEDIA_PLAY_PAUSE,
    UI_MEDIA_NEXT,
};

void ui_init();
void ui_set_volume(int pct);   /* 0–100 */
void ui_set_mute(bool muted);

/* Theme-Registry: Index 0..ui_theme_count()-1. ui_theme_set() wendet ein Theme an
 * (Hintergrund + Tick-Ring + Hint-Sichtbarkeit), auch als Live-Vorschau. */
void        ui_theme_set(int index);
int         ui_theme_get();
int         ui_theme_count();
const char* ui_theme_name(int index);

/* Theme-Switcher-Overlay (Variante B): zentrierter Name + Positions-Punkte über dem
 * Live-Hintergrund. open() merkt sich nichts — der Aufrufer (main) verwaltet das
 * Original für Abbruch. scroll() bewegt die Auswahl und wendet das Theme live an. */
void ui_switcher_open();
void ui_switcher_scroll(int delta);
int  ui_switcher_selected();
void ui_switcher_close();
bool ui_switcher_is_open();

void ui_set_connected(bool connected);   /* false -> roter Disconnect-Punkt */
void ui_flash_media(UiMediaFeedback feedback);
void ui_boot_countdown(int secondsLeft);   /* Flash-Countdown-Overlay (5..0) */
void ui_boot_flashing();                    /* Endframe vor dem Reboot: "→ FLASH" */
