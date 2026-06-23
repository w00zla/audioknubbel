# Handoff: audioknubbel Milestone 3 — C#-Companion + echter Volume-State

## Projektstatus

**Milestone 1 ✅** — USB-HID (Volume ±/Mute) + CDC-Serial, encoder/hid.
**Milestone 2 ✅ fertig & committed** — GC9A01-Display via LovyanGFX + LVGL-8.3-Arc-UI.

Das Board läuft als USB Composite Device und zeigt einen cyan Lautstärke-Bogen:
- Encoder drehen → Arc bewegt sich **+** Windows-Volume via HID (`CONSUMER_CONTROL_VOLUME_INCREMENT/DECREMENT`)
- Encoder drücken → Arc grau + rotes "MUTE" **+** Windows-Mute via HID
- Lautstärke ist **geschätzt** (lokaler Counter, 50 % Start, ±2 %/Tick) — HID kennt den echten Windows-Wert nicht

---

## ⚠️ Hardware-Lektionen aus M2 (NICHT verlieren!)

Diese kosteten Stunden Debugging — beim Anfassen von `display.cpp` beachten:

| Thema | Lektion |
|---|---|
| **Power-Rails** | **GPIO1 + GPIO2 MÜSSEN früh auf HIGH** (in `displayInit()`, vor `lcd.init()`). Das sind die Versorgungs-Enables fürs Display. Ohne sie: `lcd.init()` meldet Erfolg (240×240), aber **Panel bleibt schwarz/faint**. Das war der Root Cause des schwarzen Screens. |
| **Backlight** | GPIO46 via **LEDC-PWM** (`ledcAttach(46,5000,8)`+`ledcWrite(46,255)`). Statischer `digitalWrite(HIGH)` ergibt nur schwaches Glühen. |
| **LED-Ring** | WS2812 (Daten Pin 48) hat ein Power-Gate auf **GPIO40**: LOW = an, HIGH = aus. Aktuell HIGH (aus). Für M3-Mute-Indikator: 40 LOW + WS2812 ansteuern. |
| **SPI-Config** | Factory-Werte sind korrekt: `spi_3wire=true`, `freq_write=80MHz`. (Quelle: Elecrow-Factory-FW, `example/ESP32_Display_1_28`.) |
| **LovyanGFX** | `^1.2.0` (aktuell 1.2.21) — nötig für Arduino-Core 3.x / pioarduino 53.03.13. NICHT 1.1.5. |
| **lv_conf.h** | `LV_USE_ANIMIMG 0` zwingend (zieht sonst LV_USE_IMG als Dep → Compile-Fehler). |

---

## Aktuelle Codestruktur

```
src/
  encoder.h/cpp   — ISR-Quadratur-Decoder, encoderGetTicks(), encoderGetPress()
  hid.h/cpp       — USBHIDConsumerControl + USBCDC USBSerial, hidInit/VolumeStep/MuteToggle
  display.h/cpp   — GC9A01+LovyanGFX, LVGL-Init, Backlight-PWM, displayInit()
  ui.h/cpp        — LVGL Arc + Mute-Label, ui_init()/ui_set_volume(int)/ui_set_mute(bool)
  main.cpp        — setup()/loop(), lokaler Volume-Counter, lv_timer_handler()
  lv_conf.h       — LVGL-8.3-Config (Arc+Label, dark theme)
```

---

## Ziel Milestone 3

**Echter Windows-Volume-State aufs Display** statt geschätztem Counter — via C#-Companion-App.

### Architektur (Option B aus dem Research-Doc)
```
┌─────────────┐   USB-CDC-Serial    ┌──────────────────────┐
│  audioknubbel  │ ◀──────────────────▶ │  C#/.NET Tray-App    │
│  (ESP32-S3) │   Zeilen-Protokoll   │  + NAudio (CoreAudio)│
└─────────────┘                      └──────────────────────┘
   HID bleibt parallel aktiv (funktioniert immer, auch ohne App)
```

- **PC → Board:** echter Volume-Wert + Mute-State pushen → `ui_set_volume()`/`ui_set_mute()` zeigen exakte Werte.
- **Board → PC:** Encoder-Events (optional, falls App das Setzen übernimmt statt HID).
- **Graceful Degradation:** Läuft die App nicht, bleibt HID + lokaler Schätzwert (heutiges Verhalten).

### Vorgeschlagenes Serial-Protokoll (deej-Stil, zeilenbasiert)
```
PC → Board:   "VOL:42\n"   (0–100)        "MUTE:1\n" / "MUTE:0\n"
Board → PC:   "ENC:-2\n"   (Ticks)        "PRESS\n"
```
Parser im `loop()` auf `USBSerial.available()`; klein halten.

### PC-Seite
- **.NET 8/9**, `NAudio.CoreAudioApi`: `MMDeviceEnumerator` → Default-Device → `AudioEndpointVolume` (Master-Volume + Mute + Events bei externen Änderungen).
- `System.IO.Ports.SerialPort` für CDC (COM-Port; aktuell COM5).
- Tray-Icon: WinForms `NotifyIcon` (Autostart).
- **Stretch:** Per-App-Volume via `AudioSessionManager` (Spotify/Game/Discord durchschalten per Encoder-Druck).

---

## Build & Flash Workflow

- **pio.exe:** `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe` (nicht im PATH).
- Build: `pio.exe run -e crowpanel-s3` · Flash: `pio.exe run -e crowpanel-s3 -t upload` · Port COM5.
- **Regel:** Claude buildet/flasht, meldet "ready", User versetzt Device in Flash-Mode + "go", dann Flash.

## Nächste Session: Startpunkt
1. Serial-Protokoll-Parser in `main.cpp`/neues `protocol.cpp` (RX-Zeilen → ui_set_*).
2. C#-Tray-App-Gerüst: NAudio Master-Volume lesen + SerialPort öffnen, `VOL:`/`MUTE:` pushen.
3. Bidirektional testen, dann Per-App als Stretch.

## Referenzen
- Research-Doc: `docs/crowpanel-volume-knob-research.md` (Optionen A/B/C, deej, NAudio)
- M2-Plan + Hardware-Details: `docs/superpowers/plans/2026-06-11-milestone2-gui.md`
- Elecrow Factory-FW (Display-Config-Referenz): `Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-...`
