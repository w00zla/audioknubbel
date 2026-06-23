# Handoff — Tray-Helligkeitssteuerung (2026-06-15)

## Was umgesetzt wurde

Board-Gesamthelligkeit (Display-Backlight, GPIO46) ist jetzt über die Companion-Tray-App
einstellbar — **5–100 % in 5 %-Schritten**, board-seitig im **NVS-Flash** persistiert.
„Ganz aus" bleibt bewusst dem Standby vorbehalten (Minimum 5 %).

**Status:** Code fertig, auf `master` gemergt (Merge-Commit `5cc3d9f`). Build/Tests grün.
**Hardware-E2E steht noch aus** (beim Implementieren liefen Gerät/App parallel → kein Flash).

## Architektur / Entscheidungen

- **Board ist Quelle der Wahrheit.** Helligkeit liegt im NVS (`Preferences`, Namespace
  `audioknubbel`, Key `bright`, Default 100 %). Die App pusht nur bei Änderung und fragt den
  aktiven Wert beim Connect ab (fürs Häkchen).
- **Protokoll** (zeilenbasiert, wie gehabt):
  - `PC → Board: BRIGHT:<5-100>\n` — setzen + in NVS speichern (clamp 5..100)
  - `PC → Board: BRIGHT?\n` — abfragen → Board antwortet `BRIGHT:<n>\n`
- **Mapping** Prozent→PWM linear: `duty = round(pct/100 × 255)` (5 %→13, 100 %→255).
- **Backlight duty-basiert:** `displayBacklightSetLevel(uint8_t)` schreibt nur den Duty-Wert.
  Standby-Wake (`displayBacklightSet(true)`) stellt den **gespeicherten** Wert wieder her
  (nicht mehr hart 255). **GPIO46-Tabu respektiert** — `ledcAttach`-Config, Power-Rails
  (GPIO1/2/40) und der `displayInit`-Ablauf wurden NICHT angefasst.

## Geänderte/neue Dateien

**Firmware:**
- `src/brightness.h/.cpp` (neu) — Pure-Mapping `brightnessPctToDuty()` + NVS-API
  (`brightnessInit/Set/Get`).
- `src/display.h/.cpp` — `displayBacklightSetLevel()`, `s_bl_duty`, Wake restauriert Level.
- `src/protocol.h` — Parser für `BRIGHT:` / `BRIGHT?` (`ProtoCmd::SetBrightness/QueryBrightness`).
- `src/protocol.cpp` — Handler: `SetBrightness`→`protocolApplyBrightness`, `QueryBrightness`→
  `println("BRIGHT:n")`.
- `src/main.cpp` — `brightnessInit()` im `setup()` nach `displayInit()`;
  `protocolApplyBrightness(pct)` (= `brightnessSet` + `wakeUp`).

**Companion (.NET 10):**
- `Protocol.cs` — `BrightnessLine`, `QueryBrightnessLine`, `TryParseBrightness` (clamp 5..100).
- `SerialLink.cs` — `QueryBrightness(timeoutMs)` (Write unter `_gate`, Read danach).
- `TrayAppContext.cs` — Untermenü „Helligkeit" (20 Stufen, fett bei 5/25/50/75/100, Haken am
  aktiven Wert), Abfrage beim Connect (Background-Task), Submenu nur aktiv wenn verbunden.

**Doku:** `docs/superpowers/specs/2026-06-15-tray-brightness-design.md`,
`docs/superpowers/plans/2026-06-15-tray-brightness.md`.

## Verifikation (erledigt, ohne Gerät)

- C#-Tests: **47/47 grün** (auch nach dem Merge).
- Companion-Build (`dist`-Profil, Release single-file win-x64): 0 Fehler/Warnungen →
  `dist\AudioKnubbel.Companion.exe`. Frische Instanz läuft aktuell im Tray.
- Firmware-Build: **SUCCESS** (`.pio\build\crowpanel-s3\firmware.bin`, Flash 55.6 %).

## Offen: Hardware-E2E nach dem Flashen

1. **Flashen** (Board im Download-Modus, COM5): `pio.exe run -e crowpanel-s3 -t upload`.
   Companion hält COM5 → vorher Tray „! In Flash-Mode versetzen" ODER Companion beenden.
2. Tray → Helligkeit → Stufe wählen → Board dimmt sichtbar; Häkchen sitzt korrekt.
3. **Reboot (Power-Cycle)** → Helligkeit bleibt erhalten (NVS).
4. **Standby** (15 s Inaktivität) → Wake → kehrt zur eingestellten Helligkeit zurück (nicht 100 %).
5. Tray-Menü nach Connect öffnen → Häkchen steht auf echtem Board-Wert (`BRIGHT?`-Query).
6. Optional On-Device-Parser-Check über Serial: `BRIGHT:50` (dimmt) / `BRIGHT?` (→ `BRIGHT:50`).

Nach erfolgreichem E2E: in CLAUDE.md `Tray-Helligkeit ⏳` → `✅` umstellen.

## Bekannte Stolpersteine / Risiken

- **GPIO46-Tabu:** Nur der Duty-Wert wurde geändert. Falls das Panel nach dem Flash zickt →
  Build zurückrollen, Init-Pfad prüfen (sehr unwahrscheinlich, Attach/Rails unverändert).
- **Port-Reader:** `QueryBrightness` liest den Port; sicher, weil Heartbeat nur schreibt und
  Reconnect nur bei getrennter Verbindung läuft.
- **NVS-Schreibzyklen:** Nutzer-Klicks sind selten → kein Debounce nötig.
