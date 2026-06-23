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

**Flash-Regel (wichtig):** Claude buildet selbst und meldet „ready to flash", **flasht aber erst
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
- Commits: prägnant, mit `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- Implementierung läuft auf Feature-Branches, nicht direkt auf `master`.

## Status & Doku

- **M1 ✅** USB-HID (Volume ±/Mute) + CDC. **M2 ✅** GC9A01-Display + LVGL-Arc-UI.
  **M3 ✅** echter Volume-State via C#-Companion (NAudio, Tray, Port-Handshake).
- **Post-M3 ✅** (alles hardware-verifiziert): Tray-Icon + Volume-%-Anzeige,
  Touch-Wake aus Standby (CST816D), Media-Gesten (Double-Tap=Play/Pause,
  Swipe-L/R=Prev/Next), **Flash-Modus per Tray** (Menü „! In Flash-Mode versetzen":
  5s-Board-Countdown, dann löst die Firmware den ROM-Download-Modus selbst aus via
  `usb_persist_restart(RESTART_BOOTLOADER)` — Flash-Port dann COM5).
- **Tray-Helligkeit ✅** (hardware-verifiziert): Tray-Untermenü „Helligkeit" 5–100 % in
  5er-Schritten, board-seitig im NVS persistiert. Serial-Befehle `BRIGHT:<n>` / `BRIGHT?`;
  Backlight läuft duty-basiert (Standby-Wake stellt gespeicherten Wert wieder her).
- **Tray-Standby-Threshold ✅** (hardware-verifiziert): Tray-Untermenü „Standby" 5–60 s in
  5er-Schritten, board-seitig im NVS persistiert (`standby_cfg`). Serial-Befehle
  `STBY:<n>` / `STBY?`; Standby-Check nutzt den NVS-Wert statt Hardcode.
- **Companion-Connect-Sync:** Helligkeit + Standby werden beim Connect **streng sequenziell**
  auf einem Background-Thread abgefragt (`QueryConfigAsync`). Nie zwei parallele Port-Leser —
  sonst klauen sich die Abfragen die Antwort (concurrent `ReadLine` + `DiscardInBuffer`).
- **Theme-Switcher + Repartition ✅** (hardware-verifiziert): Single-App-`partitions.csv`
  (`factory` ~15,9 MB statt Dual-OTA 3,19 MB; `nvs` bleibt auf 0x9000 → Config überlebt).
  Theme-Registry (`ui_theme.h/.cpp`, `UiThemeDef`-Tabelle) statt hartcodierter 2-Theme-Logik;
  `s_theme` ist ein Index. Switcher-Overlay: Long-Press öffnet, Dreh = Live-Vorschau
  (entkoppelt: nur Richtung + 150 ms Cooldown, da Encoder mehrere Ticks/Rast liefert),
  **Long-Tap = übernehmen** (NVS via `theme_cfg`), Single-Tap = abbrechen. Noch nur die 2
  bestehenden Themes — neue Grafiken + feinere Stufen (15) sind eigene Folgeaufgaben.
- Specs/Pläne unter `docs/superpowers/`, Handoffs unter `docs/`. Research:
  `docs/crowpanel-volume-knob-research.md`. Letzter Handoff:
  `docs/handoff-2026-06-20-theme-switcher.md`.
