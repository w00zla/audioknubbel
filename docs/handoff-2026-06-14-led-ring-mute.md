# Handoff: Mute-LED-Ring kalibriert

Datum: 2026-06-14 · Branch: `feature/mute-led-ring-calibration`

## Ergebnis

Der WS2812-LED-Ring leuchtet bei aktivem Mute statisch rot. Der aktuell
hardware-getestete Wert ist:

```cpp
static const uint8_t MUTE_RED = 2;
```

Das ist der beste Kompromiss aus "nicht brutal hell" und "alle Ring-LEDs sichtbar".

## Was getestet wurde

- `MUTE_RED = 77` (30 %): deutlich zu hell.
- `MUTE_RED = 13` (5 %): weiterhin sehr hell.
- `MUTE_RED = 3`: ring-tauglich, aber noch heller als gewünscht.
- `MUTE_RED = 1`: zu niedrig; sichtbar war praktisch nur die untere LED.
- Glimmen `1 <-> 3`: verworfen, wirkte unruhig/nervig.
- Final: `MUTE_RED = 2`, statisch.

## Code-Stand

Nur der Rotwert in `src/led_ring.cpp` wurde angepasst. Keine zusätzliche
Timing-, Poll- oder Dimming-Logik bleibt im Code.

## Verifikation

- Firmware-Build: `pio.exe run -e crowpanel-s3`
- Geflasht und am Board getestet: `pio.exe run -e crowpanel-s3 -t upload`

