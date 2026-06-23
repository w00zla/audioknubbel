# Touch-Wake (CST816D) — Design

**Stand:** 2026-06-14 · **Branch:** `feature/touch-wake`
**Ziel:** Eine Berührung des Touch-Panels holt das Display aus dem Standby.

## Kontext

Der CrowPanel hat einen **CST816D** kapazitiven Touch-Controller (I²C, Adresse
`0x15`, Pins SDA=6 / SCL=7 / INT=5 / RST=13), der in der bisherigen Firmware
**komplett ungenutzt** ist. Standby ist in `main.cpp` gekapselt (`s_standby`,
`wakeUp()`, Backlight-aus nach 15 s); aufgeweckt wird heute nur durch
Encoder-Dreh/-Druck oder App-Push.

**Recherche-Ergebnis:** Die Elecrow-Factory-Firmware liest den CST816D per
**raw I²C (`Wire`)** über eine handgeschriebene `CST816D.h`. LovyanGFX (bereits
Dependency) bringt zwar `Touch_CST816S` mit, dessen `getTouchRaw()` liefert aber
nur x/y-Koordinaten, **nicht** das Gesten-Register `0x01`. Für Gesten-IDs muss
ohnehin selbst gelesen werden → raw I²C ist die richtige Wahl und deckt sich mit
dem handgeschriebenen/pure-Stil des Projekts (`encoderQuadStep`,
`protocolParseLine`).

## Entscheidungen

- **Scope:** Erweiterbare Basis. Der Treiber liest die volle Gesten-ID und
  exponiert sie; **verdrahtet wird vorerst nur „Berührung weckt"**.
- **Treiber:** Raw I²C via `Wire`, keine neue Dependency.
- **Wake-Trigger:** Jede Berührung (FingerNum > 0) weckt sofort — robuster als
  die im Sleep zickige Single-Click-Geste. Down-Flanke genügt.
- **Auto-Sleep des Chips deaktivieren** (Reg `0xFE = 1`): Da USB-versorgt, ist
  Strom egal; der Chip bleibt I²C-responsiv → einfaches Polling reicht, keine
  ISR/INT-Komplexität.

## Architektur

Neues Modul `src/touch.h/.cpp`, parallel zu `encoder`/`protocol`.
`display.cpp` und GPIO 1/2/40/46 bleiben unangetastet; Touch nutzt eigene Pins.

### Pure-Logik (header-only, on-device verifizierbar)

```c
enum TouchGesture { TG_NONE, TG_SWIPE_UP, TG_SWIPE_DOWN, TG_SWIPE_LEFT,
                    TG_SWIPE_RIGHT, TG_TAP, TG_DOUBLE_TAP, TG_LONG_PRESS };

// Mappt den rohen HW-Wert aus Register 0x01 auf TouchGesture.
TouchGesture touchGestureFromReg(uint8_t reg);
```

HW-Gesten-Codes (CST816): 0x01 up, 0x02 down, 0x03 left, 0x04 right,
0x05 single-click, 0x0B double-click, 0x0C long-press.

### Treiber-API (`touch.h`)

| Funktion | Zweck |
|---|---|
| `void touchInit();` | `Wire.begin(6,7)`, Reset-Puls GPIO13, Auto-Sleep aus (`0xFE=1`). |
| `bool touchGetPress();` | Pollt Chip; `true` genau **einmal pro Finger-Down-Flanke** (edge-getriggert + konsumiert, Stil von `encoderGetPress()`). Liest Reg `0x01–0x06`. |
| `TouchGesture touchLastGesture();` | Letzte HW-Gesten-ID (Fundament für künftige Gesten; jetzt ungenutzt). |

### Integration `main.cpp`

- `setup()`: `touchInit();` nach `displayInit()`.
- `loop()`: `if (s_standby && touchGetPress()) wakeUp();` — Berührung wird
  konsumiert (kein Nebeneffekt), wie Encoder-Eingaben beim Aufwachen.
  Außerhalb des Standby ist Touch vorerst no-op.

## Datenfluss

```
Finger berührt Panel → CST816D Reg 0x02 FingerNum>0
  → touchGetPress() Down-Flanke → wakeUp() → Backlight an
```

## Fehlerbehandlung

I²C-Read schlägt fehl (Chip antwortet nicht / NACK) → als „keine Berührung"
behandeln. `touchGetPress()` bleibt robust, kein Crash.

## Verifizierung

Build via `pio run -e crowpanel-s3` (kein Host-gcc → on-device-Verifikation).
Gesten-Mapping per Serial-Print prüfbar. Flash erst nach „go" vom User.
