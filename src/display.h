#pragma once
#include <stdbool.h>
#include <stdint.h>

// Initialisiert die komplette Display-Hardware + LVGL:
//   - Power-Rails (GPIO1/2) einschalten  ← ohne das bleibt das Panel dunkel!
//   - WS2812-Ring (GPIO40) abschalten
//   - GC9A01 via LovyanGFX initialisieren, Backlight-PWM an
//   - LVGL-Buffer + Display-Treiber registrieren
void displayInit();

// Backlight-Helligkeit schalten (an/aus). PWM auf GPIO46.
void displayBacklightSet(bool on);

// Backlight-Helligkeit per 8-bit-Duty (0..255) setzen. Merkt sich den Wert, damit
// displayBacklightSet(true) (Wake aus Standby) ihn wiederherstellt. PWM auf GPIO46.
void displayBacklightSetLevel(uint8_t duty);
