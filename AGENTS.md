# audioknubbel

USB-Lautstärkeregler auf Basis des **Elecrow CrowPanel 1.28" ESP32-S3 Rotary Display**.
Ein Dreh-Encoder mit rundem GC9A01-Display (240×240) regelt die Windows-Lautstärke und
zeigt sie als LVGL-Arc an. Das Board meldet sich als USB-Composite-Device (HID + CDC).

## Architektur (Big Picture)

```
Encoder dreht → HID Consumer Control → Windows-Volume ändert sich
            → (M3) C#-Tray-App liest echten Wert via NAudio
            → "VOL:n\n" über USB-CDC → Board snappt Arc auf echten Wert
```

- **HID ist der Aktuator** (stellt Windows-Volume), die C#-App ist nur **Leser + Display-Korrektor**.
- **Graceful Degradation ist Pflicht:** Ohne laufende App bleibt HID + lokaler Schätzwert (50 % Start,
  ±2 %/Raststellung) voll funktionsfähig. Das Board sendet im Normalbetrieb **keine** Events zur App.

## Code-Struktur (`src/`)

| Datei | Zweck |
|---|---|
| `encoder.h/.cpp` | ISR-Quadratur-Decoder. `encoderQuadStep()` ist **pure** (header-only, `#ifdef ARDUINO` trennt den Board-Teil). |
| `hid.h/.cpp` | `USBHIDConsumerControl` (Volume ±/Mute) + `USBCDC USBSerial`. `hidInit()` ruft `USB.begin()`. |
| `display.h/.cpp` | GC9A01 via LovyanGFX + LVGL-Init. **Hardware-Tabu — siehe unten.** |
| `ui.h/.cpp` | LVGL Arc + Mute-Label. API: `ui_init()`, `ui_set_volume(int 0-100)`, `ui_set_mute(bool)`. |
| `protocol.h` | (M3) **pure** Serial-Parser `protocolParseLine()` für `VOL:`/`MUTE:`/`ID?`. |
| `main.cpp` | `setup()`/`loop()`: Encoder lesen → UI + HID, lokaler Volume-Schätzwert, `lv_timer_handler()`. |
| `lv_conf.h` | LVGL-8.3-Config (Arc+Label, dark theme). `LV_USE_ANIMIMG 0` zwingend. |

Die C#-Companion-App lebt (ab M3) in `companion/` (.NET 8 WinForms Tray, NAudio).

## Build & Flash

`pio.exe` ist **nicht im PATH** — Vollpfad nutzen:
`C:\Users\w00zla\.platformio\penv\Scripts\pio.exe`

- **Build:** `pio.exe run -e crowpanel-s3`
- **Flash:** `pio.exe run -e crowpanel-s3 -t upload` (Port **COM5**, 115200)
- **Monitor:** `pio.exe device monitor -e crowpanel-s3`

**Flash-Regel (wichtig):** Codex buildet selbst und meldet „ready to flash", **flasht aber erst
nach explizitem „go" vom User** — der versetzt das Device manuell in den Download-Modus. Build,
Monitor und C#-Builds brauchen kein „go".

## ⚠️ Stolpersteine (teuer erkauft — nicht erneut reintappen)

- **Display-Power-Rails:** `GPIO1` + `GPIO2` müssen **früh in `displayInit()` auf HIGH** (vor `lcd.init()`),
  sonst bleibt das Panel schwarz, obwohl `lcd.init()` Erfolg meldet. War der Root Cause des schwarzen
  Screens in M2. **`display.cpp` und GPIO 1/2/40/46 nicht anfassen.**
- **Backlight:** GPIO46 via LEDC-PWM (`ledcAttach(46,5000,8)`+`ledcWrite(46,255)`), nicht `digitalWrite`.
- **LED-Ring:** WS2812 (Daten GPIO48) hat Power-Gate auf GPIO40 (LOW=an, HIGH=aus).
- **Platform:** community `pioarduino`-Fork (Arduino-Core 3.x), nicht offizielles `espressif32`.
  LovyanGFX **`^1.2.0`** (nicht 1.1.5). LVGL `~8.3.11`.
- **Kein Host-gcc/g++ auf dieser Maschine** (nur Xtensa-ESP-Toolchain). PlatformIO `[env:native]`
  Unity-Tests laufen hier **nicht** — wurde in M1 bewusst entfernt. Pure C++-Logik (`encoderQuadStep`,
  `protocolParseLine`) wird stattdessen **on-device** verifiziert. C#-xUnit-Tests laufen (dotnet 10 installiert).
- **Theme-Artwork-Lektion (1/5-Sterne-Debakel, 2026-06-21):** User-Vorgaben zu visuellen
  Assets sind strikt zu befolgen. Nicht "clever" per Script retten, wenn der User sagt,
  dass die Artwork-Stufen selbst kuratiert/gebaut werden sollen. Die 15 Background-Stufen
  eines Themes dürfen **nicht** per Script künstlerisch erzeugt, aufgefüllt, reveal-maskiert,
  umsortiert, normalisiert oder sonstwie inhaltlich verändert werden. Scripts sind nur für
  technische Schritte erlaubt: Zuschnitt, Kern-/Icon-Pad-Abdunklung, Kontaktbogen/Preview,
  Hash/Archivierung und RGB565/LVGL-Konvertierung.
- **Theme-Workflow-Gate:** Bei neuen Theme-BGs zuerst nur visuelle Asset-Arbeit. Keine
  Firmware-Integration, kein `src/ui_backgrounds.cpp`, kein PlatformIO-Build, bevor der User
  die 15er-Sequenz ausdrücklich freigegeben hat. Kontaktbogen muss zeigen: rohe Stufen und
  grobes Firmware-Overlay. Prüfen: keine gecroppten Kreise, keine wandernde Füllung, klare
  Progression "mehr Volume = mehr BG".
- **Stop heißt Stop:** Wenn der User sagt, dass der Ansatz falsch ist, nicht in derselben
  Richtung weiteroptimieren. Erst innehalten, die Anweisung in eigenen Worten bestätigen und
  nur mit explizit passendem nächsten Schritt weitermachen.
- **Eskalationsmarker:** Wenn Codex bei Theme-Artwork erneut versucht, User-Vorgaben mit
  Script-Magie zu umgehen oder nach sichtbaren Warnzeichen trotzdem baut/konvertiert, den
  User daran erinnern, dass er darum gebeten hat, ans OpenAI-Abo-Kündigen erinnert zu werden.

## Serial-Protokoll (M3)

Zeilenbasiert, ASCII, `\n`-terminiert. Unbekannte/leere Zeilen ignorieren, `\r\n` tolerieren,
`VOL:` auf 0–100 clampen, Overflow-Zeilen verwerfen.

```
PC → Board:   VOL:<0-100>\n   MUTE:<0|1>\n   ID?\n
Board → PC:   AUDIOKNUBBEL <fw>\n   (nur als Antwort auf ID?)
```

## Konventionen

- **Pure Logik separat halten:** Geschäftslogik in header-only/`#ifdef ARDUINO`-getrennte Funktionen,
  damit sie ohne Board nachvollziehbar/testbar ist (siehe `encoderQuadStep`, `protocolParseLine`).
- Commits: prägnant, mit `Co-Authored-By: Codex Opus 4.8 <noreply@anthropic.com>`.
- Implementierung läuft auf Feature-Branches, nicht direkt auf `master`.

## Status & Doku

- **M1 ✅** USB-HID (Volume ±/Mute) + CDC. **M2 ✅** GC9A01-Display + LVGL-Arc-UI.
- **M3 🔧 in Arbeit** (Branch `milestone3-companion`): echter Volume-State via C#-Companion.
- Specs/Pläne unter `docs/superpowers/`, Handoffs unter `docs/`. Research:
  `docs/crowpanel-volume-knob-research.md`. Aktueller M3-Plan:
  `docs/superpowers/plans/2026-06-11-milestone3-companion.md`.
