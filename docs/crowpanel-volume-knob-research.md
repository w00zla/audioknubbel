# CrowPanel 1.28" Rotary → Volume/Mute-Controller — Recherche

**Stand:** 2026-06-10 · **Ziel:** Volume + Mute für Windows-Audio mit dem [CrowPanel 1.28" HMI ESP32 Rotary Display](https://www.elecrow.com/crowpanel-1-28inch-hmi-esp32-rotary-display-240-240-ips-round-touch-knob-screen.html)

---

## Hardware-Fakten (verifiziert)

| Komponente | Detail |
|---|---|
| MCU | **ESP32-S3R8** (Dual-Core LX7 @ 240 MHz, 8 MB PSRAM, 16 MB Flash) |
| Display | 1,28" IPS rund, 240×240, **GC9A01** via SPI |
| Touch | kapazitiv, **CST816D** via I²C |
| USB | USB-C hängt am **nativen S3-USB** (Board-Setting „USB CDC on Boot" in den Tutorials → kein CH340-Bridge-Chip) ⇒ **USB-HID möglich!** |
| Funk | WiFi 2,4 GHz + BLE 5.0 |
| Extras | 5× WS2812 RGB-LED-Ring, 2× UART, I²C, FPC |

**Pinout** (aus dem [Makerguides-Tutorial](https://www.makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display/)):

```
Display GC9A01:  SCLK=10, MOSI=11, DC=3, CS=9, RST=14, Backlight=46
Touch CST816D:   SDA=6, SCL=7, INT=5, RST=13
Encoder:         A=45, B=42, Switch(Press)=41
RGB-LED WS2812:  Pin 48 (5 LEDs)
```

---

## Architektur: 3 Optionen

### Option A — USB-HID Consumer Control ⭐ Empfehlung als Startpunkt
Das Panel meldet sich am PC als Media-Tastatur und schickt `VOLUME_INCREMENT` / `VOLUME_DECREMENT` / `MUTE` — exakt das, was die Lautstärketasten einer Tastatur tun.

- **Null PC-Software nötig.** Funktioniert an jedem Rechner, auch am Fernseher.
- Arduino-ESP32-Core bringt `USBHIDConsumerControl` fertig mit (`#include "USBHIDConsumerControl.h"`, `CONSUMER_CONTROL_VOLUME_INCREMENT` etc.).
- Einschränkung: HID kennt nur relative Steps + Mute-Toggle. Der **aktuelle** Lautstärkewert ist am Gerät nicht bekannt → Anzeige auf dem Display ist „geschätzt" oder man verzichtet auf die Prozentzahl.

### Option B — Companion-App in C# (dein Sweet Spot)
ESP32 redet über **USB-CDC-Serial** (oder WiFi/WebSocket) mit einer kleinen .NET-Tray-App. Die App nutzt die **Windows Core Audio API** via [NAudio](https://github.com/naudio/NAudio) (`MMDeviceEnumerator`, `AudioSessionManager`).

- **Bidirektional:** PC pusht echten Volume-Wert + Mute-State + App-Namen zurück → Display zeigt exakte Prozent, App-Icon, Mute-Status.
- **Per-App-Volume:** Knopf drücken/touchen → App durchschalten (Spotify, Game, Discord), drehen → nur deren Session regeln. Genau das Feature, das deej groß gemacht hat — nur mit C# statt Go.
- Mehraufwand: App muss laufen (Autostart-Tray, ~200 Zeilen NAudio-Code).

### Option A+B kombiniert (der eigentliche Endausbau)
Der S3 kann ein **USB-Composite-Device** sein: HID **und** CDC-Serial gleichzeitig. HID macht Volume/Mute (funktioniert immer, auch ohne App), die C#-App liefert on top Zustand + Per-App-Steuerung, wenn sie läuft. Graceful Degradation gratis.

### Option C — ESPHome (nur falls Home Assistant im Spiel)
Fertige Configs existieren ([HA-Community-Thread zum CrowPanel 1.28](https://community.home-assistant.io/t/crowpanel-1-28inch-esp32-rotary-display-with-esphome/960928), [KrX3D/WaveShare-Knob-Esp32S3](https://github.com/KrX3D/WaveShare-Knob-Esp32S3) für fast identische Hardware inkl. Media-Player-Volume-Arc). Für direkte **PC**-Audio-Steuerung aber der Umweg über HA — nicht dein Use-Case, nur der Vollständigkeit halber.

---

## Code-Vorlagen (sortiert nach Nützlichkeit)

### Für genau dieses Board
1. **[Elecrow-RD GitHub-Repo (offiziell)](https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen)** — `example/`, `factory_sourcecode/` (die Demo-Firmware mit LVGL-UI inkl. Volume-Screen!), Eagle-Schematics, Datasheets. **Erste Anlaufstelle** — die Factory-Firmware ist quasi schon eine Volume-UI-Vorlage.
2. **[Elecrow Wiki Arduino-Lessons](https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display_Arduino_lesson1.html)** — Schritt-für-Schritt: Display, Touch, Encoder, LVGL.
3. **[Makerguides Getting Started](https://www.makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display/)** — bestes unabhängiges Tutorial. Beispiel 3 (Encoder regelt LED-Helligkeit, interrupt-basiertes Quadratur-Decoding) ist **1:1 die Vorlage für Volume** — nur den Output tauschen. Library-Versionen: ESP32-Core 3.3.2, Arduino_GFX 1.6.2.
4. **[TasteTheCode-Review mit Projektideen](https://www.tastethecode.com/exploring-the-potential-of-the-crowpanel-esp32-128-display-for-diy-projects)** — Hinweis: bei Compile-Problemen mit den Elecrow-Beispielen ESP32-Core 2.0.10 + LovyanGFX 1.1.5 + LVGL 8.3.x pinnen.

### Volume-Knob-Logik (andere Hardware, Konzept übertragbar)
5. **[0xa10/volume.control](https://github.com/0xa10/volume.control)** — minimale USB-HID-Consumer-Control-Firmware: Drehen = Vol ±, Drücken = Mute. Genau Option A, nur ohne Display.
6. **[deej](https://github.com/omriharel/deej)** (12k+ Stars) — der Klassiker für Per-App-Volume via Serial + PC-Client (Go). Das **Serial-Protokoll und die Session-Matching-Logik** sind die Blaupause für Option B; den Go-Client ersetzt du durch C#/NAudio. Forks: [deej-esp32](https://github.com/outoftune2000/deej-esp32), [deej-plus (UDP/WiFi)](https://github.com/Lefuneste83/deej-plus).
7. **[KrX3D/WaveShare-Knob-Esp32S3](https://github.com/KrX3D/WaveShare-Knob-Esp32S3)** — ESPHome-Config für baugleiche S3-Knob-Hardware mit Volume-Arc-UI; gute UI-Inspiration auch wenn du nicht ESPHome nimmst.

---

## Dev-Env-Empfehlung (zugeschnitten auf .NET/C# + TS, Windows)

### Firmware: **VS Code + PlatformIO + Arduino-Framework (C++)**
- C++ mit Arduino-API liest sich für einen C#-Dev sofort flüssig; ESP-IDF (reines C, FreeRTOS-Boilerplate) wäre unnötige Reibung für dieses Projekt.
- PlatformIO statt Arduino IDE: echte Dependency-Pins in `platformio.ini` (reproduzierbare Builds statt „Library Manager-Roulette"), IntelliSense, integrierter Serial-Monitor — fühlt sich nach `csproj` an, nicht nach Bastelbude.
- ⚠️ **Stolperstein:** Die offizielle `espressif32`-Platform in PlatformIO hängt beim Arduino-Core 2.x fest. Für Core 3.x (aktuelle USB-HID-APIs, von Makerguides verwendet) den community-gepflegten **[pioarduino-Fork](https://github.com/pioarduino/platform-espressif32)** als Platform eintragen — ein Einzeiler in der `platformio.ini`.
- Fallback: Arduino IDE 2.x funktioniert auch (alle Elecrow-Tutorials nutzen sie), skaliert aber schlechter, sobald das Projekt mehr als eine `.ino` ist.

### UI: **LVGL** + UI-Designer
- LVGL (8.3 oder 9.x — Version an die gewählten Beispiele anpassen, nicht mischen!) für Volume-Arc, Mute-Icon, App-Anzeige.
- **[EEZ Studio](https://github.com/eez-open/studio)** (Open Source) oder **SquareLine Studio** (Freemium, von Elecrow offiziell supportet): Drag&Drop-UI-Designer, generieren LVGL-C-Code. Für eine runde 240×240-Volume-UI ist Handcoding aber auch in <100 Zeilen machbar.

### PC-Seite (Option B): **.NET 8/9 Tray-App + NAudio**
- `NAudio.CoreAudioApi`: `MMDeviceEnumerator` → Default-Device → `AudioEndpointVolume` (Master) bzw. `AudioSessionManager` → Sessions (per App). Mute, Volume, Events bei externen Änderungen — alles dabei.
- `System.IO.Ports.SerialPort` für die USB-CDC-Verbindung; simples Zeilen-Protokoll à la deej (`VOL:0.42`, `MUTE:1`, `APP:Spotify`).
- Tray-Icon: WinForms `NotifyIcon` reicht, oder [H.NotifyIcon](https://github.com/HavenDV/H.NotifyIcon) für WPF/WinUI.

### Was ich **nicht** empfehle
- **.NET nanoFramework** (C# direkt auf dem ESP32-S3): klingt nach deinem Stack, aber GC9A01/LVGL-Support ist dünn — du würdest Display-Treiber selbst zusammenzimmern. Der C#-Spaß gehört auf die PC-Seite.
- **MicroPython:** geht, aber LVGL-Bindings auf dem S3 sind Gefrickel und Performance auf 240×240 mäßig.

---

## Vorgeschlagene Roadmap

1. **Factory-Firmware anschauen** (`factory_sourcecode/` im Elecrow-Repo) — verstehen, wie Display + Encoder + LVGL dort verdrahtet sind.
2. **Milestone 1 (ein Abend):** PlatformIO-Projekt, Encoder → `USBHIDConsumerControl` (Vol ±/Mute), simple LVGL-Arc-Anzeige. → Option A fertig, ab da täglich nutzbar.
3. **Milestone 2:** C#-Tray-App mit NAudio, CDC-Serial-Protokoll, echter Volume-State aufs Display.
4. **Milestone 3 (optional):** Per-App-Sessions, Touch/Press zum App-Wechsel, RGB-Ring als Mute-Indikator (rot = stumm).

## Quellen

- [Elecrow Wiki — CrowPanel 1.28" HMI](https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html)
- [Elecrow-RD GitHub-Repo](https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen)
- [Makerguides — Getting Started CrowPanel 1.28"](https://www.makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display/)
- [TasteTheCode — CrowPanel Projektideen](https://www.tastethecode.com/exploring-the-potential-of-the-crowpanel-esp32-128-display-for-diy-projects)
- [deej](https://github.com/omriharel/deej) · [deej-esp32](https://github.com/outoftune2000/deej-esp32) · [deej-plus](https://github.com/Lefuneste83/deej-plus)
- [0xa10/volume.control](https://github.com/0xa10/volume.control)
- [KrX3D/WaveShare-Knob-Esp32S3](https://github.com/KrX3D/WaveShare-Knob-Esp32S3)
- [HA-Community — CrowPanel 1.28 mit ESPHome](https://community.home-assistant.io/t/crowpanel-1-28inch-esp32-rotary-display-with-esphome/960928)
- [pioarduino platform-espressif32](https://github.com/pioarduino/platform-espressif32)
