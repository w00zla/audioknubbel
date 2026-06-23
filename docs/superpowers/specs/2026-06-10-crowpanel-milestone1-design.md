# Design: audioknubbel Firmware — Milestone 1 (Minimal)

**Datum:** 2026-06-10  
**Status:** Approved  
**Ziel:** PlatformIO-Projekt für den CrowPanel 1.28" ESP32-S3 — Encoder → USB-HID Consumer Control (Volume ±, Mute) + USB-CDC Serial Debug. Kein Display/LVGL in diesem Milestone.

---

## Scope

In scope:
- `platformio.ini` mit pioarduino-Fork (ESP32 Core 3.x)
- Encoder-Modul: ISR-basiertes Quadratur-Decoding, Ticks + Press-Events
- HID-Modul: `USBHIDConsumerControl`-Wrapper (Volume ±, Mute)
- USB Composite: CDC (Serial Debug) + HID gleichzeitig
- `main.cpp`: 100 Hz Polling-Loop, Serial-Debug-Ausgabe

Out of scope:
- Display (GC9A01), Touch (CST816D), LVGL → Milestone 2
- CDC-Serial-Protokoll zum PC / C#-Tray-App → Milestone 3
- Per-App-Volume, RGB-Ring → Milestone 3

---

## Hardware

| Komponente | Detail |
|---|---|
| MCU | ESP32-S3R8 (Dual-Core LX7 @ 240 MHz, 8 MB PSRAM, 16 MB Flash) |
| USB | Nativ (kein CH340) — USB-C direkt am S3 |
| Encoder A | Pin 45 |
| Encoder B | Pin 42 |
| Encoder Switch | Pin 41 |

---

## Projektstruktur

```
audioknubbel/
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── encoder.cpp
│   ├── encoder.h
│   ├── hid.cpp
│   └── hid.h
└── docs/
```

---

## platformio.ini

```ini
[env:crowpanel-s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.13/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

**Begründung pioarduino-Fork:** Die offizielle `espressif32`-Platform in PlatformIO ist auf Arduino Core 2.x eingefroren. `USBHIDConsumerControl` ist erst ab Core 3.x stabil verfügbar. Der pioarduino-Fork liefert Core 3.x mit identischer `platformio.ini`-Syntax.

**Begründung build_flags:** `ARDUINO_USB_MODE=1` aktiviert native USB (HWCDC). `ARDUINO_USB_CDC_ON_BOOT=1` registriert zusätzlich einen CDC-Port — so laufen CDC (Serial) und HID als USB-Composite-Device gleichzeitig auf dem einen USB-C-Anschluss.

---

## Modul: encoder

### Interface (`encoder.h`)

```cpp
void encoderInit();
int  encoderGetTicks();   // akkumulierte Ticks seit letztem Aufruf, atomarer Reset
bool encoderGetPress();   // true einmalig pro Drück-Flanke
```

### Verhalten

- ISR an Pin 45 (A) und 42 (B): 4-Zustand-Quadratur-Tabelle, kein Delay-Debounce
- Shared `portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;` für atomaren Zugriff
- In der ISR: `portENTER_CRITICAL_ISR(&encMux)` / `portEXIT_CRITICAL_ISR(&encMux)`
- In `encoderGetTicks()` (Main-Loop): `portENTER_CRITICAL(&encMux)` / `portEXIT_CRITICAL(&encMux)` — **nicht** `_ISR`-Variante, die ist nur innerhalb von ISRs erlaubt
- Pin 41 (Switch): Flanken-ISR, setzt ein einmaliges Press-Flag; Software-Debounce über Timestamp (min. 50 ms zwischen zwei Press-Events)
- Positives Tick-Vorzeichen = CW = Lautstärke hoch; negativ = CCW = Lautstärke runter

---

## Modul: hid

### Interface (`hid.h`)

```cpp
void hidInit();
void hidVolumeStep(int ticks);   // |ticks| Reports senden, Vorzeichen bestimmt Richtung
void hidMuteToggle();
```

### Verhalten

- `hidInit()` instanziiert `USBHIDConsumerControl`, ruft danach `USB.begin()` auf — das registriert alle USB-Interfaces (HID + CDC) beim Host. `USB.begin()` muss **vor** `Serial.begin()` stehen, da `Serial` auf dem selben USB-Stack sitzt
- `hidVolumeStep(ticks)`: schickt für jeden Tick einen separaten HID-Report (`CONSUMER_CONTROL_VOLUME_INCREMENT` oder `CONSUMER_CONTROL_VOLUME_DECREMENT`) gefolgt von einem Release-Report
- `hidMuteToggle()`: schickt `CONSUMER_CONTROL_MUTE` + Release

---

## main.cpp — Loop-Logik

```
setup():
  hidInit()           // USB.begin() intern → muss zuerst kommen
  Serial.begin(115200)
  Serial.setTxTimeoutMs(0)   // verhindert Blockierung wenn kein Serial Monitor offen
  encoderInit()

loop():
  ticks = encoderGetTicks()
  if ticks != 0:
    Serial.printf("[ENC] ticks=%d\n", ticks)
    hidVolumeStep(ticks)
  if encoderGetPress():
    Serial.println("[ENC] press → mute")
    hidMuteToggle()
  delay(10)   // ~100 Hz
```

---

## Verifikation / Definition of Done

- [ ] `pio run` baut ohne Fehler
- [ ] Board erscheint am PC als HID-Gerät (Device Manager: HID-konformes Gerät)
- [ ] Board erscheint gleichzeitig als virtueller COM-Port
- [ ] Drehen → Windows-Lautstärke ändert sich
- [ ] Drücken → Windows-Mute togglet
- [ ] Serial Monitor zeigt `[ENC] ticks=...` bei Drehung

---

## Nächste Milestones (out of scope hier)

- **Milestone 2:** GC9A01-Display-Init + LVGL 8.3.x + Volume-Arc-Widget
- **Milestone 3:** CDC-Serial-Protokoll + C#-NAudio-Tray-App + Per-App-Sessions + RGB-Ring
