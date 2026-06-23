#pragma once
#include <stdint.h>

// Pure: Helligkeit in Prozent (wird auf 5..100 geklemmt) auf 8-bit-PWM-Duty
// abbilden. Linear: 5 % -> 13, 100 % -> 255. Header-only & ohne Arduino, damit
// on-device/pur nachvollziehbar (wie encoderQuadStep/protocolParseLine).
inline uint8_t brightnessPctToDuty(int pct) {
    if (pct < 5)   pct = 5;
    if (pct > 100) pct = 100;
    return (uint8_t)((pct * 255 + 50) / 100);
}

#ifdef ARDUINO
// NVS-gestützte Helligkeit. brightnessInit() lädt den Wert (Default 100) und
// wendet ihn aufs Backlight an. brightnessSet() clamped, wendet an, speichert.
void brightnessInit();
void brightnessSet(int pct);
int  brightnessGet();
#endif
