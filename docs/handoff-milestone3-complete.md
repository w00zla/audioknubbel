# Handoff: audioknubbel Milestone 3 — ABGESCHLOSSEN

## Projektstatus

**Milestone 1 ✅** — USB-HID (Volume ±/Mute) + CDC-Serial.
**Milestone 2 ✅** — GC9A01-Display via LovyanGFX + LVGL-8.3-Arc-UI.
**Milestone 3 ✅ fertig, E2E-verifiziert & nach `master` gemergt** (Merge-Commit `9ca9241`, `--no-ff`).

Der Lautstärke-Arc zeigt jetzt den **echten** Windows-Master-Volume + Mute, geliefert von
einer .NET-Tray-App über USB-CDC-Serial. **Graceful Degradation** bleibt: ohne laufende App
funktioniert HID + lokaler Schätzwert weiter wie in M2.

---

## Architektur (wie gebaut) — Hybrid Closed Loop

```
┌─────────────┐   USB (Composite)    ┌──────────────────────────┐
│  audioknubbel  │  HID  ───────────────▶  Windows Volume
│  (ESP32-S3) │  CDC  ◀───VOL:/MUTE:── │  AudioKnubbel.Companion      │
│             │  ──────ID?/AUDIOKNUBBEL──▶ │  (.NET 10 WinForms Tray) │
└─────────────┘                       │  + NAudio (CoreAudio)    │
                                       └──────────────────────────┘
```

- **Encoder = Aktuator** (HID stellt Windows-Volume, wie M2). Die App ist **nur Leser +
  Display-Korrektor**: NAudio meldet den echten Wert (auch bei externen Änderungen), die App
  pusht `VOL:`/`MUTE:` aufs Board, das seinen Schätzwert damit überschreibt (snap to truth).
- Board sendet im Normalbetrieb **keine** Events zur App — der Regelkreis schließt über NAudio.

### Serial-Protokoll (zeilenbasiert, ASCII, `\n`)
```
PC → Board:   VOL:<0-100>\n   MUTE:<0|1>\n   ID?\n
Board → PC:   AUDIOKNUBBEL M3\n   (nur als Antwort auf ID?)
```
Robust: unbekannte/leere Zeilen ignoriert, `\r\n` toleriert, `VOL:` auf 0–100 geclampt,
Overflow-Zeilen verworfen.

---

## Codestruktur (Ist-Zustand)

**Firmware (`src/`)** — unverändert aus M2 plus:
```
protocol.h    — pure Parser protocolParseLine() (VOL:/MUTE:/ID?), #ifdef ARDUINO trennt protocolPoll()
protocol.cpp  — protocolPoll(): liest USBSerial zeilenweise -> protocolApplyVolume/Mute / ID-Reply
main.cpp      — protocolApplyVolume/Mute (überschreiben Schätzwert + ui_set_*), protocolPoll() im loop()
hid.cpp       — USB.productName("audioknubbel") vor USB.begin() (Discovery-Tiebreaker)
```

**C#-Companion (`companion/`)** — .NET 10 WinForms, NAudio 2.2.1, System.IO.Ports 10, System.Management 10:
```
Protocol.cs         VOL:/MUTE:-Formatierung mit Clamp (pure, getestet)
Contracts.cs        AudioState, IVolumeSource, ISerialSink
SyncController.cs   Dedup pro Feld + Full-State on first/Reset, nichts senden wenn disconnected (getestet)
PortDiscovery.cs    COM-Port via WMI über Espressif-VID 0x303A
SerialLink.cs       SerialPort 115200, ISerialSink, thread-safe, Auto-Reconnect
VolumeMonitor.cs    NAudio Master-Volume/Mute, OnVolumeNotification, Default-Device-Follow
TrayAppContext.cs   Verdrahtung: VolumeMonitor -> 40ms-Coalescing -> SyncController -> SerialLink
                    + NotifyIcon (Status/Reconnect/Exit) + 2s-Reconnect-Timer
Program.cs          Entry + Single-Instance-Mutex
global.json         pinnt SDK auf 10.0.x
Tests/              xUnit: ProtocolTests (7) + SyncControllerTests (6) = 13 grün
```

---

## Build / Run / Flash

**Firmware** (`pio.exe` Vollpfad, nicht im PATH):
```
C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3            # build
C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3 -t upload  # flash
```
**Flash-Regel:** Claude buildet + meldet „ready", User setzt Device in Download-Modus + „go",
dann erst Upload.

**Companion:**
```
dotnet build companion/AudioKnubbel.Companion.sln
dotnet test  companion/AudioKnubbel.Companion.sln     # 13 Tests
dotnet run --project companion/AudioKnubbel.Companion # Tray-App
```

---

## ⚠️ Stolpersteine (teuer erkauft — nicht erneut reintappen)

| Thema | Lektion |
|---|---|
| **COM-Port** | `USB.productName` hat den CDC-Port **COM5 → COM6** umnummeriert (im Betrieb). Unkritisch — App findet ihn per VID/PID. Für manuelle Serial-Tests jetzt **COM6** (PowerShell `System.IO.Ports.SerialPort`; offener `pio device monitor` blockiert → „Access denied"). |
| **WinForms-NRE** | `SynchronizationContext.Current` ist im `TrayAppContext`-Ctor **null** (Ctor läuft vor `Application.Run`). Crashte beim Start, sobald das Board sofort gefunden wurde. Fix: UI-Marshalling über verstecktes `Control` + `BeginInvoke` (Handle im Ctor erzwungen). |
| **NAudio-Deadlock** | Re-Attach an neues Default-Device aus `IMMNotificationClient.OnDefaultDeviceChanged` **deadlockte** — re-entranter `GetDefaultAudioEndpoint` auf dem Notification-Thread. Fix: `Task.Run(Attach)`. **Regel: nie synchron aus IMMNotificationClient-Callbacks in den Enumerator zurückrufen.** |
| **Kein Host-gcc** | PlatformIO `[env:native]` (Unity) läuft auf dieser Maschine nicht — in M1 (`713bf63`) bewusst entfernt. Pure C++-Logik (`protocolParseLine`) wird **on-device** verifiziert, nicht via native Unit-Test. C#-xUnit ist davon unberührt. |
| **LSP-Rauschen** | Editor-LSP meldet dauerhaft `CS0518 System.Object nicht definiert` — fehlende net10-Referenzauflösung im Language-Server, **kein** echter Fehler (CLI-Build/Tests grün). Ggf. OmniSharp/Roslyn neu starten. |
| **Hardware-Tabu (M2)** | `display.cpp` + GPIO 1/2/40/46 nicht anfassen (GPIO1/2 = Display-Power-Rails). |

---

## Testing

- **Firmware:** on-device Serial-Test (`VOL:`/`MUTE:`/`ID?` → Arc/Reply). Kein native Unit-Test (s.o.).
- **C#:** 13 xUnit-Tests (Protocol-Format + SyncController-Logik). NAudio/Serial/Tray manuell.
- **E2E (alle bestanden):** Arc zeigt echten Volume · folgt Windows-Slider · folgt HID-Encoder ·
  Mute · Output-Device-Wechsel · Exit → HID-Fallback · Auto-Reconnect nach Replug.

---

## Offene Punkte / Nächste Schritte (M4-Kandidaten)

- **Auto-Start** — bewusst Out-of-Scope in M3. Einzeiler: Registry `HKCU\...\Run`-Key oder
  Startup-Shortcut. Optional + Tray-Toggle.
- **Per-App-Volume** (Stretch aus der M3-Spec) — Encoder-Druck schaltet zwischen
  Audio-Sessions (Spotify/Game/Discord), regelt deren Einzel-Volume via `AudioSessionManager`.
  Bräuchte erweitertes Protokoll (App-Name/Index aufs Display) + Board→PC-Events für die Auswahl.
- **LED-Ring** als Mute-Indikator — WS2812 (Daten GPIO48), Power-Gate GPIO40 (LOW=an). Aktuell aus.
- **Single-Exe-Distribution** — `dotnet publish -r win-x64 --self-contained` + ggf. Installer.

---

## Referenzen

- Design-Spec M3: `docs/superpowers/specs/2026-06-11-milestone3-companion-design.md`
- Implementierungsplan M3: `docs/superpowers/plans/2026-06-11-milestone3-companion.md`
- Pre-Impl-Handoff M3: `docs/handoff-milestone3.md`
- Projekt-Guide: `CLAUDE.md`
- Research (Optionen A/B/C, deej, NAudio): `docs/crowpanel-volume-knob-research.md`
