# Milestone 3 — C#-Companion + echter Volume-State (Design-Spec)

**Datum:** 2026-06-11
**Status:** Approved (Design), Implementierung ausstehend
**Vorgänger:** Milestone 2 (`9a96c96`) — Display + LVGL-Arc-UI fertig

## Ziel

Der Lautstärke-Arc auf dem audioknubbel-Display soll den **echten** Windows-Master-Volume +
Mute-State zeigen statt des heutigen lokalen Schätzwerts. Eine C#/.NET-Tray-App liest den
Zustand via NAudio (CoreAudio) und pusht ihn über USB-CDC-Serial aufs Board.

**Graceful Degradation ist nicht verhandelbar:** Läuft die App nicht, bleibt das heutige
Verhalten (HID + lokaler Schätzwert) voll funktionsfähig.

## Architektur — Hybrid, Closed Loop

Der Encoder bleibt der **Aktuator** (HID, wie heute). Die App ist reiner **Leser +
Display-Korrektor**. Der Regelkreis schließt sich über NAudio — das Board sendet **keine**
Events zur App:

```
Encoder → HID → Windows-Volume ändert sich → NAudio OnVolumeNotification
        → App → "VOL:n\n" → Board überschreibt Schätzwert (snap to truth)
```

Beim Drehen aktualisiert das Board den Arc sofort optimistisch auf seinen lokalen
Schätzwert (Instant-Feedback); kurz darauf korrigiert die einlaufende `VOL:`-Zeile auf den
exakten Wert.

### Entscheidungen (aus Brainstorming)

| Frage | Entscheidung |
|---|---|
| Aktuator / Source of Truth | **HID stellt, App liest** (Hybrid) — erhält Graceful Degradation |
| Port-Discovery | **Auto-Detect via VID/PID** (+ Product-String als Tiebreaker) |
| Update-Strategie | **Event-getrieben + Debounce + Full-State-on-Connect** (kein Polling) |
| Kern-Scope | Master-Volume + Mute, externe Änderungen reflektieren, Output-Device-Wechsel folgen |
| Stretch (nicht in M3) | Per-App-Volume |

## Serial-Protokoll

Zeilenbasiert, ASCII, `\n`-terminiert.

```
PC → Board:   VOL:<0-100>\n     Setzt angezeigte Lautstärke
              MUTE:<0|1>\n       Setzt Mute-Zustand
              ID?\n              (optional) Identitäts-Abfrage
Board → PC:   AUDIOKNUBBEL <fw>\n    (nur als Antwort auf ID?)
```

**Robustheit:** unbekannte und leere Zeilen werden ignoriert; `\r\n` wird toleriert;
Zeilenlänge wird gecappt (Overflow → Zeile verwerfen). Out-of-range `VOL:`-Werte werden auf
0–100 geclampt.

## Firmware-Änderungen (ESP32-S3)

### Neu: `src/protocol.h` / `src/protocol.cpp`
- `protocolPoll()` — liest `USBSerial.available()`, akkumuliert Bytes in einen Zeilenpuffer
  bis `\n`, übergibt komplette Zeilen an den Parser.
- `protocolParseLine(const char* line, ...)` — **pure Funktion**, isoliert testbar. Erkennt
  `VOL:`, `MUTE:`, `ID?`. Liefert geparstes Kommando + Wert zurück; Anwendung (ui_set_* /
  Reply) erfolgt im Aufrufer.
- Bei `VOL:` → `ui_set_volume(v)` **und** lokalen `s_volume_pct` überschreiben.
- Bei `MUTE:` → `ui_set_mute(b)` **und** lokalen `s_muted` überschreiben.
- Bei `ID?` → `USBSerial.println("AUDIOKNUBBEL <fw>")`.

### `src/main.cpp`
- `loop()` ruft `protocolPoll()` pro Iteration auf (vor `lv_timer_handler()`).
- Lokaler Schätzwert + optimistisches Update beim Drehen bleiben unverändert (Fallback).
- Der lokale Volume-/Mute-State muss vom Protokoll-Parser überschreibbar sein (gemeinsame
  Quelle, nicht zwei divergierende Zähler).

### `platformio.ini`
- USB-Descriptor setzen, damit die App eindeutig matchen kann:
  `USB_PRODUCT="audioknubbel"` (Build-Flag). VID bleibt Espressif `0x303A`.
- Hinweis: Descriptor-Änderung kann die COM-Neunummerierung auslösen — unkritisch, da
  Discovery über VID/PID läuft.

### Hardware-Tabus (aus M2, NICHT verletzen)
- GPIO1 + GPIO2 müssen früh HIGH bleiben (Power-Rails). `display.cpp` nicht anfassen.
- Backlight GPIO46 via LEDC-PWM. LED-Ring-Power-Gate GPIO40.

## C#-Tray-App

**Stack:** .NET 8, `net8.0-windows`, WinForms (nur `NotifyIcon` via `ApplicationContext`,
kein Hauptfenster), NuGet `NAudio`. Neuer Ordner `companion/` im Repo.

```
companion/
  AudioKnubbel.Companion.sln
  AudioKnubbel.Companion/
    AudioKnubbel.Companion.csproj   (net8.0-windows, WinForms, NAudio)
    Program.cs                  Entry + Single-Instance-Mutex
    TrayAppContext.cs           ApplicationContext: Verdrahtung + NotifyIcon + Debounce
    VolumeMonitor.cs            NAudio: Volume/Mute lesen, Events, Device-Wechsel
    SerialLink.cs               SerialPort: senden, Auto-Reconnect
    PortDiscovery.cs            COM-Port via VID/PID + Product-String finden
```

### Komponenten & Verantwortlichkeiten

- **`VolumeMonitor`** — kapselt `MMDeviceEnumerator` + `AudioEndpointVolume` des
  Default-Render-Geräts. Public: `int Volume` (0–100), `bool IsMuted`, Event `StateChanged`.
  Abonniert `OnVolumeNotification` (fängt HID- **und** externe Slider-Änderungen).
  Implementiert `IMMNotificationClient` → bei Default-Device-Wechsel re-attach an neues Gerät
  + `StateChanged` feuern. Wirft/fängt `COMException` sauber, wenn ein Gerät verschwindet.
- **`PortDiscovery`** — sucht via WMI (`Win32_PnPEntity`) nach VID `0x303A` (+ Product-String
  „audioknubbel" als Tiebreaker), liefert den COM-Port-Namen.
- **`SerialLink`** — öffnet `SerialPort`, sendet Zeilen, Auto-Reconnect per Retry-Timer
  (~2 s) bei Disconnect/Replug. Optional `ID?`→`AUDIOKNUBBEL`-Verify. Public: `bool Connected`,
  `Send(string line)`, Event `ConnectionChanged`.
- **`TrayAppContext`** — `ApplicationContext`. Verdrahtet `VolumeMonitor.StateChanged`
  → Debounce (~40 ms) → `SerialLink.Send("VOL:.."/"MUTE:..")`. Sendet bei jedem (Re-)Connect
  einmal vollen State. NotifyIcon-Kontextmenü: Status (connected/searching), Reconnect, Exit.
- **`Program`** — Entry-Point, Single-Instance via Named Mutex, startet `TrayAppContext`.

### Datenfluss

1. **Connect:** aktuellen Volume+Mute lesen → Full-State (`VOL:`, `MUTE:`) senden.
2. **NAudio-Event** (HID-Encoder oder externer Slider): Debounce → bei Änderung `VOL:`/`MUTE:`.
3. **Device-Wechsel:** re-attach → neu lesen → Full-State senden.
4. **Serial-Disconnect:** Tray zeigt „disconnected", Retry-Loop; Board fällt auf lokalen
   Schätzwert zurück (Graceful Degradation).

### Fehlerbehandlung

- Serial-Write-Fehler → `Connected = false`, Reconnect-Loop starten.
- Kein Board gefunden → Tray „suche…", Discovery periodisch wiederholen.
- Audio-Device weg → `COMException` fangen, auf `IMMNotificationClient`-Callback warten.
- Debounce verhindert Serial-Flut bei schnellen NAudio-Event-Bursts.

## Testing

- **Firmware:** `protocolParseLine()` als pure Funktion isoliert testbar — idealerweise
  PlatformIO `native`-Unit-Test (VOL/MUTE/ID?/Müll/Overflow). Mindestens manueller
  Serial-Monitor-Test (`VOL:30` → Arc springt auf 30 %).
- **C#:** `VolumeMonitor` und `SerialLink` hinter Interfaces → Wiring + Debounce mit Fakes
  unit-testbar. E2E manuell: (a) Windows-Slider ziehen → Arc folgt; (b) Knopf drehen → Arc
  snappt von Schätzwert auf echten Wert; (c) App beenden → Knopf funktioniert weiter
  (HID-Fallback); (d) USB neu stecken → App reconnectet automatisch.

## Out of Scope (Stretch / spätere Milestones)

- Per-App-Volume (Encoder-Druck schaltet zwischen App-Sessions, regelt deren Einzel-Volume).
- Autostart-Installer / Tray-Settings-UI über das Minimum hinaus.
- Board→PC-Event-Stream (im Hybrid-Modell nicht nötig).

## Referenzen

- Handoff: `docs/handoff-milestone3.md`
- Research (Optionen A/B/C, deej, NAudio): `docs/crowpanel-volume-knob-research.md`
- M2-Hardware-Lektionen: `docs/superpowers/plans/2026-06-11-milestone2-gui.md`
