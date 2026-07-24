# audioknubbel 🔊

Ein **USB-Lautstärkeregler** auf Basis des *Elecrow CrowPanel 1.28" ESP32-S3 Rotary Display*.
Ein Dreh-Encoder mit rundem 240×240-IPS-Display regelt die Windows-Lautstärke und zeigt sie
als animierten LVGL-Arc an. Das Board meldet sich als **USB-Composite-Device** (HID + CDC) —
Plug-and-Play, ohne Treiber. Eine optionale C#-Tray-App synchronisiert die Anzeige mit dem
echten Windows-Volume und bringt Extras wie Helligkeits- und Standby-Einstellungen.
Themes werden direkt am Board umgeschaltet (Long-Press am Encoder).

---

## Hardware

Das Projekt läuft auf dem **Elecrow CrowPanel 1.28" HMI ESP32 Rotary Display**:

🔗 <https://www.elecrow.com/crowpanel-1-28inch-hmi-esp32-rotary-display-240-240-ips-round-touch-knob-screen.html>

| Merkmal | Detail |
|---|---|
| **MCU** | ESP32-S3 (Dual-Core, USB-OTG, PSRAM) |
| **Display** | 1,28" rundes IPS, GC9A01, **240 × 240 px** |
| **Touch** | Kapazitiver Touch-Controller (CST816D) |
| **Bedienung** | Physischer Dreh-Encoder mit Rast + Push-Button, um den Bildschirm rotierbar |
| **Extras** | WS2812-LED-Ring, USB-C |
| **Flash** | 16 MB |

Angesteuert wird das Panel mit **LovyanGFX** (`^1.2.0`) + **LVGL** (`~8.3.11`).

---

## Funktionsprinzip

```
Encoder dreht → HID Consumer Control → Windows-Volume ändert sich
            → (optional) C#-Tray-App liest echten Wert via NAudio
            → "VOL:n\n" über USB-CDC → Board snappt Arc auf echten Wert
```

- **HID ist der Aktuator** — der Encoder stellt direkt die Windows-Lautstärke (Volume ±/Mute).
- Die **C#-App ist nur Leser + Display-Korrektor** — sie zwingt dem Board nichts auf, sondern
  korrigiert nur den angezeigten Wert auf das echte Windows-Volume.
- **Graceful Degradation ist Pflicht:** Ohne laufende App bleibt das Board voll funktionsfähig
  (HID + lokaler Schätzwert: 50 % Start, ±2 %/Raststellung). Im Normalbetrieb sendet das Board
  **keine** Events an die App.

---

## Features

- 🎚️ **Lautstärke drehen** — flüssiger LVGL-Arc mit Prozent-Anzeige
- 🔇 **Mute** per Encoder-Klick, mit LED-Ring- und Display-Rückmeldung
- 👆 **Touch-Gesten** — Double-Tap = Play/Pause, Swipe-L/R = Prev/Next, Touch-Wake aus Standby
- 🎨 **Themes** — mehrere Display-Designs, verwaltet über eine Theme-Registry (`ui_theme.*`).
  **Long-Press** am Encoder öffnet das Switcher-Overlay, **Drehen** = Live-Vorschau,
  **Long-Tap** = übernehmen (im NVS persistiert), **Single-Tap** = abbrechen.
- 🖥️ **C#-Tray-Companion** (.NET, WinForms, NAudio):
  - Tray-Icon mit Live-Volume-%
  - Helligkeit (5–100 %) und Standby-Timeout (5–60 s) einstellbar — board-seitig im NVS persistiert
  - Flash-Modus per Tray-Menü (5s-Countdown, Board löst ROM-Download-Modus selbst aus)
- 💾 **Persistente Config** — Helligkeit, Standby-Schwelle und Theme überleben Neustarts (NVS)

---

## Projektstruktur

```
audioknubbel/
├─ src/                     Firmware (ESP32-S3, Arduino + LVGL)
│  ├─ main.cpp              setup()/loop(): Encoder → UI + HID, Volume-Schätzwert
│  ├─ encoder.*             ISR-Quadratur-Decoder (pure Logik: encoderQuadStep)
│  ├─ hid.*                 USB HID Consumer Control + USB-CDC Serial
│  ├─ display.*             GC9A01 via LovyanGFX + LVGL-Init  ⚠️ Hardware-Tabu
│  ├─ ui.* / ui_theme.*     LVGL-Arc, Mute-Label, Theme-Registry
│  ├─ touch.*               CST816D Touch + Media-Gesten
│  ├─ led_ring.*            WS2812-LED-Ring
│  ├─ protocol.*            Serial-Parser (pure: protocolParseLine)
│  └─ brightness / standby / theme_cfg   NVS-Config
├─ companion/               C#-Tray-App (.NET, NAudio) + xUnit-Tests
├─ docs/                    Specs, Pläne, Handoffs
├─ partitions.csv           Single-App-Layout (factory ~15,9 MB, nvs auf 0x9000)
└─ platformio.ini           pioarduino-Fork (Arduino-Core 3.x)
```

---

## Build & Flash

Empfohlen wird **VS Code mit der PlatformIO-Extension** — bauen, flashen und der serielle
Monitor laufen dann direkt über die PlatformIO-Toolbar. Alternativ per CLI:

```bash
# Firmware bauen
pio run -e crowpanel-s3

# Flashen (115200) — Board vorher in den Download-Modus versetzen
pio run -e crowpanel-s3 -t upload

# Serieller Monitor
pio device monitor -e crowpanel-s3
```

**Companion-App:**

```bash
cd companion
dotnet build          # oder: dotnet test  (xUnit)

# Release-Build erzeugen (Framework-abhängige Single-File-EXE nach ./dist/)
dotnet publish AudioKnubbel.Companion -p:PublishProfile=dist
```

Das Publish-Profil `dist` (`Properties/PublishProfiles/dist.pubxml`) baut eine
framework-abhängige `win-x64`-Single-File-EXE und legt sie unter `dist/` im Repo-Root ab.

---

## Serial-Protokoll

Zeilenbasiert, ASCII, `\n`-terminiert. Unbekannte/leere Zeilen werden ignoriert, `\r\n`
toleriert, `VOL:` auf 0–100 geclampt.

```
PC → Board:   VOL:<0-100>    MUTE:<0|1>    ID?
              BRIGHT:<n>     BRIGHT?
              STBY:<n>       STBY?
Board → PC:   AUDIOKNUBBEL <fw>          (nur als Antwort auf ID?)
```

---

## Konventionen

- **Pure Logik separat halten** — Geschäftslogik in header-only / `#ifdef ARDUINO`-getrennte
  Funktionen, damit sie ohne Board nachvollziehbar bleibt (`encoderQuadStep`, `protocolParseLine`).
- Auf dieser Maschine gibt es **kein Host-gcc/g++** — reine C++-Logik wird **on-device** verifiziert.
  C#-Tests (xUnit) laufen dagegen normal.
- Implementierung läuft auf Feature-Branches, nicht direkt auf `master`.

---

## ⚠️ Stolpersteine (teuer erkauft)

- **Display-Power-Rails:** `GPIO1` + `GPIO2` müssen **früh in `displayInit()` auf HIGH** (vor
  `lcd.init()`), sonst bleibt das Panel schwarz. **`display.cpp` und GPIO 1/2/40/46 nicht anfassen.**
- **Backlight:** GPIO46 via LEDC-PWM, nicht `digitalWrite`.
- **LED-Ring:** WS2812 (GPIO48) hat ein Power-Gate auf GPIO40 (LOW = an).
- **Platform:** community `pioarduino`-Fork (Arduino-Core 3.x), **nicht** offizielles `espressif32`.

---

## Status

- **M1 ✅** USB-HID (Volume ±/Mute) + CDC
- **M2 ✅** GC9A01-Display + LVGL-Arc-UI
- **M3 ✅** echter Volume-State via C#-Companion (NAudio, Tray, Port-Handshake)
- **Post-M3 ✅** Tray-Icon + Volume-%, Touch-Wake, Media-Gesten, Flash-Modus per Tray,
  Helligkeits- & Standby-Einstellung (NVS), board-seitiger Theme-Switcher

Details, Specs und Handoffs unter [`docs/`](docs/).
