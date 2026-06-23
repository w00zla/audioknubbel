#pragma once
#include <stdint.h>

// ── CST816D Touch-Controller (I²C 0x15) ──────────────────────────────────────
// Pins: SDA=6, SCL=7, INT=5 (ungenutzt, wir pollen), RST=13.
// Der Chip wird in touchInit() aus dem Auto-Sleep geholt, damit Polling über
// Wire zuverlässig antwortet (USB-versorgt → Stromverbrauch egal).

// HW-Gesten-IDs des CST816 (Register 0x01). Berührung weckt aus Standby;
// im wachen Zustand steuern Double-Tap/Swipe-L/R die Medienwiedergabe.
enum TouchGesture {
    TG_NONE = 0,
    TG_SWIPE_UP,
    TG_SWIPE_DOWN,
    TG_SWIPE_LEFT,
    TG_SWIPE_RIGHT,
    TG_TAP,
    TG_DOUBLE_TAP,
    TG_LONG_PRESS,
};

// Pure Logik (header-only, on-device verifizierbar wie encoderQuadStep):
// mappt den rohen Wert aus CST816-Register 0x01 auf TouchGesture.
constexpr TouchGesture touchGestureFromReg(uint8_t reg) {
    switch (reg) {
        case 0x01: return TG_SWIPE_UP;
        case 0x02: return TG_SWIPE_DOWN;
        case 0x03: return TG_SWIPE_LEFT;
        case 0x04: return TG_SWIPE_RIGHT;
        case 0x05: return TG_TAP;
        case 0x0B: return TG_DOUBLE_TAP;
        case 0x0C: return TG_LONG_PRESS;
        default:   return TG_NONE;
    }
}

constexpr bool touchGestureIsDefinitive(TouchGesture g) {
    return g == TG_SWIPE_LEFT || g == TG_SWIPE_RIGHT ||
           g == TG_SWIPE_UP   || g == TG_SWIPE_DOWN  ||
           g == TG_DOUBLE_TAP || g == TG_LONG_PRESS;
}

static_assert(touchGestureFromReg(0x0C) == TG_LONG_PRESS, "CST816 0x0C maps to long press");
static_assert(touchGestureIsDefinitive(TG_LONG_PRESS), "long press should finalize without waiting for settle timeout");

#ifdef ARDUINO
// I²C starten, Chip resetten, Auto-Sleep deaktivieren.
void touchInit();

// Einmal pro Loop aufrufen: liest den Chip (ein I²C-Read) und aktualisiert die
// Press-/Gesten-Events. touchTake* holen diese danach ab.
void touchPoll();

// Finger-Down-Flanke (für Wake). Liefert true GENAU einmal pro Berührung,
// konsumiert (im Stil von encoderGetPress()).
bool touchTakePress();

// Abgeschlossene Geste beim Finger-Lift — einmal, konsumiert; sonst TG_NONE.
TouchGesture touchTakeGesture();

// Verwirft ausstehende Touch-Events und den aktuellen Touch-Zyklus. Wird genutzt,
// wenn ein Encoder-Press Vorrang hat und keine Mediengeste auslösen soll.
void touchCancelGesture();
#endif
