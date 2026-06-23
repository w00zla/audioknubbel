# Handoff — Tray-Standby-Threshold + Helligkeit hardware-verifiziert (2026-06-15)

## Was umgesetzt wurde

Zwei Tray-Konfig-Features sind jetzt **hardware-verifiziert** und auf `master`:

1. **Tray-Helligkeit ✅** (Backlight, GPIO46): Untermenü „Helligkeit" 5–100 % in 5er-Schritten,
   NVS-persistiert. (Details: `docs/handoff-2026-06-15-tray-brightness.md`.)
2. **Tray-Standby-Threshold ✅** (neu): Untermenü „Standby" 5–60 s in 5er-Schritten,
   board-seitig im NVS persistiert.

**Status:** beide auf `master` gemergt, Build/Tests grün, **E2E am Gerät bestätigt** (Dimmen,
Standby-Timing, NVS-Persistenz nach Reboot, korrekte Häkchen nach App-Neustart).

## Standby-Threshold — Architektur

- **Board = Quelle der Wahrheit** (NVS, `Preferences` Namespace `audioknubbel`, Key `standby`,
  Default 15 s). Spiegelt das Helligkeits-Pattern.
- **Protokoll:** `PC → Board: STBY:<5-60>\n` (setzen+speichern), `STBY?\n` (abfragen) →
  Board antwortet `STBY:<n>\n`. Clamp 5..60.
- **Kein Hardware-Seiteneffekt** beim Setzen (anders als Backlight) — der Wert wirkt nur als
  Schwelle im Standby-Check.

### Geänderte/neue Dateien (Standby)

- `src/standby_cfg.h/.cpp` (neu) — NVS-API `standbyCfgInit/Set/Get` (Sekunden).
- `src/protocol.h` — Parser `STBY:` / `STBY?` (`ProtoCmd::SetStandby/QueryStandby`, clamp 5..60).
- `src/protocol.cpp` — `STBY:` → `protocolApplyStandby`; `STBY?` → `println("STBY:n")`.
- `src/main.cpp` — `STANDBY_TIMEOUT_MS`-Hardcode entfernt; Check nutzt
  `standbyCfgGet() * 1000UL`; `standbyCfgInit()` im `setup()`; `protocolApplyStandby(sec)`
  (= `standbyCfgSet` + `wakeUp`).
- Companion: `Protocol.cs` (`StandbyLine`/`QueryStandbyLine`/`TryParseStandby`),
  `SerialLink.cs` (`QueryStandby`), `TrayAppContext.cs` (Untermenü „Standby", 5/15/30/60 fett).

## Wichtiger Fix: Connect-Sync der Config-Abfragen

**Bug:** Beim Connect liefen Helligkeits- und Standby-Abfrage als **zwei parallele** `Task.Run`
auf demselben `SerialPort`. Folgen: gleichzeitiges `ReadLine()` (nicht thread-safe) routete die
Antworten falsch zu, und das `DiscardInBuffer()` der zweiten Abfrage verwarf die schon
eingetroffene Antwort der ersten → eine/beide Abfragen liefen in den Timeout → Menü blieb auf
dem Default-Wert (100 % / 15 s) stehen.

**Fix:** Beide Abfragen jetzt **streng sequenziell** auf **einem** Background-Thread
(`QueryConfigAsync` in `TrayAppContext.cs`): Helligkeit ganz lesen, dann Standby. Damit gilt
wieder die Single-Reader-Invariante wie bei `Handshake`/`RequestBootCountdown`.

**Lehre:** Auf dem Port darf zu jedem Zeitpunkt nur **ein** Leser aktiv sein. Jede neue
Port-lesende Operation muss entweder vor `connected=true` laufen oder die anderen Port-Nutzer
(Heartbeat/Reconnect/andere Queries) serialisieren.

## UI-Politur (gleicher Lauf)

- Menü-**Separator über „Helligkeit"**.
- Flash-Item liegt jetzt unter einem eigenen **„DEVELOPER"**-Hauptpunkt (Submenu), damit
  „! In Flash-Mode versetzen" nicht versehentlich angeklickt wird.
- **DEVELOPER hinter Build-Flag `DEVELOPER_MENU`:** Submenu **und** `EnterBootloader()` sind
  per `#if DEVELOPER_MENU` umschlossen — bei Aus wird der Flash-Code komplett rauskompiliert
  (kein Dead-Code). Gesteuert über die csproj-Property **`EnableDeveloperMenu` (Default
  `true`)**, die `DEVELOPER_MENU` für **alle Targets** setzt (`dotnet run`, `dotnet build`,
  `dotnet publish`). Für einen End-User-Build ohne Developer-Menü:
  `dotnet publish AudioKnubbel.Companion -p:PublishProfile=dist -p:EnableDeveloperMenu=false`.
  Beide Varianten bauen sauber (0/0), Tests 64/64.

## Verifikation

- C#-Tests: **64/64 grün** (auch nach Merge).
- Companion-Build (`dist`-Profil): 0 Fehler/Warnungen.
- Firmware-Build: **SUCCESS** (Flash ~55 %).
- Hardware-E2E: vom User bestätigt (Helligkeit + Standby inkl. korrektem Häkchen-Sync nach
  App-Neustart).

## Offene Punkte / Hinweise

- **Keine automatisierten Tests** fürs Serial-Concurrency-Verhalten: `TrayAppContext` erzeugt
  `SerialLink` + WinForms direkt im Ctor (keine DI-Naht), Query-Methoden liegen nicht hinter
  einem Interface. Ein Regression-Test bräuchte ein Refactoring zur Dependency-Injection.
  Abgesichert ist die Parse-Ebene (xUnit) + manueller Smoke-Test.
- Companion läuft als single-instance (Mutex). Vor erneutem Publish: alte Instanz beenden,
  sonst ist die EXE gesperrt. Start aus `dist\AudioKnubbel.Companion.exe`.
