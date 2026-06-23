# Handoff 2026-06-23 — Rename crow-knob → audioknubbel + Tray-Reconnect-Fix

## Was passiert ist

### 1. Projekt-Rename + neues Repo
Das Projekt heißt jetzt **audioknubbel** und lebt in einem **frischen Git-Repo** unter
`D:\ProjectsLocal\audioknubbel` (ohne History, ein Initial-Commit). Das alte
`D:\ProjectsLocal\crow-knob` bleibt unverändert als Backup liegen.

Übernommen via `git archive HEAD | tar -x` (nur getrackte Dateien, kein
`.git`/`.pio`/`bin`/`obj`/`dist`-Ballast).

**Umbenannt (Projekt-Identität):**
- C#-Namespace/Projekte: `CrowKnob.Companion` → `AudioKnubbel.Companion`
  (Solution, beide `.csproj`, Ordner, Namespaces, `RootNamespace`, ProjectReference,
  Autostart-Registry-Value, Single-Instance-Mutex, Embedded-Resource-Name).
- Wire-Token (Serial-Handshake): `CROWKNOB M3` → `AUDIOKNUBBEL M3`
  (`src/protocol.cpp` **und** `Protocol.cs`/Tests — beide Seiten matchen).
- NVS-Namespace + USB-Manufacturer: `crowknob` → `audioknubbel`
  (`src/brightness.cpp`, `standby_cfg.cpp`, `theme_cfg.cpp`, `hid.cpp`).
- Icons: `crow-knob.ico` → `audioknubbel.ico`, `resources/crow-knob-icon.png`
  → `resources/audioknubbel-icon.png`.
- Docs/CLAUDE.md/AGENTS.md durchgängig.

**Bewusst NICHT angefasst (Hardware, kein Projektname):**
- `Elecrow` (Vendor), `crowpanel-s3` (PlatformIO-Env / Board-Name), `CrowPanel`.

**Konsequenzen (bekannt & akzeptiert):**
- Board musste **neu geflasht** werden — sonst antwortet die alte FW mit `CROWKNOB`
  und die umbenannte Companion erkennt sie nicht. ✅ erledigt (FW läuft).
- NVS-Namespace-String hat sich geändert → Helligkeit/Standby/Theme standen **einmalig
  auf Default**. Danach persistiert wieder normal.

### 2. Fix: Tray-App teilweise unresponsive ohne Board
**Root cause:** `_reconnect` ist ein `System.Windows.Forms.Timer` → `Tick` läuft auf dem
**UI-Thread** und rief `TryConnect()` synchron auf. Bei getrenntem Board blockiert das
den UI-Thread pro Versuch: `PortDiscovery.FindPort()` (WMI) + `Handshake()` mit
`Thread.Sleep(250)` + bis zu 3×3 `ReadLine`-Timeouts à 500 ms = **bis ~4,75 s**, wenn ein
Espressif-Port existiert, aber nicht antwortet. Verbunden short-circuitet der Tick —
darum nur „unresponsive **wenn kein Board verbunden**".

**Fix** (`TrayAppContext.cs`): Connect-Versuch via `Task.Run` auf Background-Thread
(Muster wie `QueryConfigAsync`/`EnterBootloader`), `_reconnecting`-Guard gegen
überlappende Versuche; auch der erste Ctor-Versuch läuft im Hintergrund.
`ConnectionChanged` marshallt sich ohnehin via `Post`/`BeginInvoke` selbst zurück.

## Verifikation
- ✅ Firmware `crowpanel-s3` baut (`[SUCCESS]`, Flash 67,2 %, RAM 34,5 %), auf Board geflasht.
- ✅ Companion: Solution baut, **64/64 Tests grün** (Protocol/SyncController).
- ✅ Release-Exe: `dist\AudioKnubbel.Companion.exe` (Single-File, win-x64, framework-dependent).
- ⚠️ Tray-Responsiveness ist WinForms-Threading → nicht automatisiert testbar.
  **Offen zu bestätigen:** neue Exe ohne Board starten und prüfen, dass Menü/Tooltip/Klicks
  flüssig bleiben. Falls noch Haken: nächster Verdacht ist der `Send`-WriteTimeout im
  Heartbeat (500 ms), aber das feuert nur bei Connected.

## Commits (master)
- `chore: initial audioknubbel (renamed from crow-knob)`
- `fix(companion): Reconnect-Versuch nicht mehr auf dem UI-Thread`

## Build & Flash (unverändert)
- Build FW: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
- Flash: `… run -e crowpanel-s3 -t upload --upload-port COM5` (Board vorher via `BOOT?`
  in den Flash-Modus; CLI-Trigger siehe unten).
- Companion: `dotnet test companion\AudioKnubbel.Companion.sln` /
  `dotnet publish companion\AudioKnubbel.Companion\AudioKnubbel.Companion.csproj -p:PublishProfile=dist`

### Flash-Modus per CLI (Reset-Button abgeklebt)
PowerShell `System.IO.Ports.SerialPort` auf dem aktuellen Port (115200, `DtrEnable=$true`),
**ca. 1 s settlen**, dann `BOOT?\n` schreiben + flushen, bis zu 10 s auf `BOOTREADY` lesen.
Das Board spielt 5 s Countdown und löst den ROM-Download-Modus selbst aus → re-enumeriert
als Flash-Port (zuletzt COM5). Settle-Zeit ist wichtig: ohne genug DTR-Settle kommt kein
BOOTREADY (einmal beobachtet, beim zweiten Versuch mit 1 s Settle ok).
