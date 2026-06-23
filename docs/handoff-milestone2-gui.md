# Handoff: audioknubbel Milestone 2 — GUI (Display + LVGL)

## Projektstatus

**Milestone 1 ✅ fertig und committed** (`master`, 4 Commits)

Das Board läuft als USB Composite Device:
- Encoder dreht → `CONSUMER_CONTROL_VOLUME_INCREMENT/DECREMENT` via USB HID
- Encoder drückt → `CONSUMER_CONTROL_MUTE` via USB HID
- `USBSerial` (explicit USBCDC) läuft parallel als COM-Port für Debug

**Wichtige Lektion aus M1:** `ARDUINO_USB_CDC_ON_BOOT=1` reicht auf Windows nicht — explizite `USBCDC USBSerial`-Instanz in `hid.cpp` nötig.

---

## Hardware-Pinout (verifiziert)

```
Display GC9A01:  SCLK=10, MOSI=11, DC=3, CS=9, RST=14, Backlight=46
Touch CST816D:   SDA=6, SCL=7, INT=5, RST=13
Encoder:         A=45, B=42, Switch=41   ← bereits in encoder.cpp verdrahtet
RGB WS2812:      Pin 48, 5 LEDs
```

---

## Aktuelle Codestruktur

```
src/
  encoder.h/cpp   — ISR-Decoder, encoderGetTicks(), encoderGetPress()
  hid.h/cpp       — USBHIDConsumerControl + USBCDC USBSerial, hidInit/VolumeStep/MuteToggle
  main.cpp        — setup()/loop(), 100 Hz polling
```

`main.cpp` `loop()` gibt akkumulierte Ticks weiter — das ist der Hook für die Display-Anzeige: lokalen Volume-Counter hochzählen und ans LVGL-Widget übergeben.

---

## Ziel Milestone 2

Rundes 240×240 Display zeigt:
- **Arc-Widget** (LVGL `lv_arc`) als Lautstärke-Indikator
- **Mute-Icon** wenn gemutet
- Keine Prozentzahl nötig (HID kennt echten Windows-Wert nicht → geschätzter lokaler Counter)

---

## Tech-Entscheidungen für M2

### Display-Library
Zwei Optionen — **eine wählen, nicht mischen:**

| | LovyanGFX | Arduino_GFX |
|---|---|---|
| LVGL-Integration | sehr gut, eigener Treiber | gut, via `Arduino_GFX_Library` |
| Makerguides-Tutorial | ✓ (Core 2.x Beispiel, aber portierbar) | ✓ (Core 3.x) |
| Empfehlung | wenn LVGL-Performance wichtig | wenn einfacher Start |

**Empfehlung: LovyanGFX 1.1.5** — beste LVGL-Integration, viele GC9A01-Beispiele.

### LVGL Version
**8.3.x** — nicht 9.x. Alle Elecrow-Beispiele und die Factory-Firmware nutzen 8.3.
Mischen verboten → in `platformio.ini` pinnen: `bodmer/TFT_eSPI` NICHT verwenden.

### lib_deps für platformio.ini
```ini
lib_deps =
    lovyan03/LovyanGFX @ ^1.1.5
    lvgl/lvgl @ ~8.3.11
```

### LVGL-Config
LVGL braucht eine `lv_conf.h` — entweder im `src/`-Verzeichnis oder über `build_flags = -I src`.
Minimale Config für 240×240:
```c
#define LV_HOR_RES_MAX  240
#define LV_VER_RES_MAX  240
#define LV_COLOR_DEPTH  16
#define LV_TICK_CUSTOM  1
```

---

## Vorgeschlagene Dateistruktur M2

```
src/
  display.h/cpp   — GC9A01-Init via LovyanGFX, LVGL-Setup, lv_task_handler()-Aufruf
  ui.h/cpp        — LVGL-Widgets: Arc + Mute-Icon, ui_set_volume(int pct), ui_set_mute(bool)
  main.cpp        — erweitert: lokaler volume_pct counter, Aufruf ui_set_*
  lv_conf.h       — LVGL-Konfiguration
```

---

## Logik-Erweiterung in main.cpp

```cpp
static int s_volume_pct = 50;   // Startwert, geschätzt
static bool s_muted = false;

// in loop():
int ticks = encoderGetTicks();
if (ticks != 0) {
    s_volume_pct = constrain(s_volume_pct - ticks * 2, 0, 100);
    ui_set_volume(s_volume_pct);
    hidVolumeStep(-ticks);
}
if (encoderGetPress()) {
    s_muted = !s_muted;
    ui_set_mute(s_muted);
    hidMuteToggle();
}
lv_task_handler();   // LVGL tick — muss regelmäßig aufgerufen werden
```

---

## Referenzen

- **Elecrow Factory-Firmware** (LVGL-Beispiel für genau dieses Board): [github.com/Elecrow-RD/CrowPanel-1.28inch...](https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen) → `factory_sourcecode/`
- **Makerguides Tutorial** (Display + Touch + Encoder Step-by-Step): [makerguides.com/getting-started-crowpanel-1-28inch...](https://www.makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display/)
- **LovyanGFX GC9A01 Config**: im Repo unter `examples/` für ESP32-S3 SPI

---

## Nächste Session: Startpunkt

1. Factory-Firmware kurz ansehen — verstehen wie LovyanGFX + LVGL dort verdrahtet sind
2. `lv_conf.h` + `display.cpp` aufsetzen, Display-Init testen (weißer Screen = Erfolg)
3. LVGL-Arc-Widget in `ui.cpp`, `ui_set_volume()` implementieren
4. In `main.cpp` einhängen
