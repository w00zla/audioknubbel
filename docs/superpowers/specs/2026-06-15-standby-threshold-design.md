# Standby-Threshold übers Tray — Design

**Datum:** 2026-06-15
**Branch:** `feature/standby-threshold`
**Status:** Freigegeben (User), bereit für Implementierungsplan

## Ziel

Über die Companion-Tray-App soll der **Standby-Timeout** des Boards einstellbar sein —
**5–60 s in 5-s-Schritten**, board-seitig im **NVS-Flash** persistiert (Quelle der Wahrheit,
Graceful Degradation). Spiegelt 1:1 das Helligkeits-Feature
(siehe `2026-06-15-tray-brightness-design.md`).

## Verhalten / Wertebereich

- Standby-Timeout **5–60 s** in **5-s-Schritten** (12 Stufen).
- Default auf jungfräulichem Board (NVS leer): **15 s** (bisheriger Hardcode-Wert).
- Untermenü „Standby": Stufen-Label `"<n> s"`, **fett** bei **5, 15, 30, 60**, Haken am aktiven Wert.
- Kein Hardware-Seiteneffekt beim Setzen (anders als Backlight): der Wert wirkt nur als Schwelle
  im Standby-Check.

## Serial-Protokoll (Erweiterung)

Zeilenbasiert, ASCII, `\n`-terminiert. Wert in **Sekunden**.

```
PC → Board:   STBY:<5-60>\n     setzen + in NVS speichern
PC → Board:   STBY?\n           aktuellen Wert abfragen
Board → PC:   STBY:<n>\n         Antwort auf STBY?
```

- Clamping: `STBY:` wird auf **5..60** geklemmt.
- Unbekannte/leere Zeilen weiterhin ignorieren; `\r\n` tolerieren; Overflow-Zeilen verwerfen.

## Firmware

### `protocol.h` (pure Parser)

- Neue Enum-Werte `ProtoCmd::SetStandby` und `ProtoCmd::QueryStandby`.
- `protocolParseLine()` erkennt `STBY?` (Query) und `STBY:` (Set, clamp 5..60).
  Reihenfolge: `STBY?` vor `STBY:` prüfen.
- Pure, on-device verifizierbar (kein Host-gcc).

### `standby_cfg.h/.cpp` (neues Modul, analog `brightness.*`)

- Hält den aktuellen Timeout (Sekunden) im RAM.
- NVS via `Preferences`: Namespace `audioknubbel`, Key `standby` (eigener Handle).
  - `standbyCfgInit()` → lädt NVS, Default 15, clamp 5..60.
  - `standbyCfgSet(sec)` → clamp 5..60, speichern.
  - `standbyCfgGet()` → aktueller Wert (Sekunden); für `STBY?`-Antwort und den Standby-Check.

### `main.cpp`

- `static const uint32_t STANDBY_TIMEOUT_MS = 15000;` **entfällt**.
- Standby-Check nutzt `(uint32_t)standbyCfgGet() * 1000UL`.
- `setup()`: `standbyCfgInit()` nach `brightnessInit()`.
- `protocolApplyStandby(sec)` = `standbyCfgSet(sec); wakeUp();` (Config-Push setzt den
  Aktivitäts-Timer zurück, damit die neue Schwelle ab jetzt frisch zählt).

### `protocol.cpp`

- `STBY:` (`SetStandby`) → `protocolApplyStandby(value)`.
- `STBY?` (`QueryStandby`) → `USBSerial.println("STBY:" + standbyCfgGet())`.

## Companion-App

### `Protocol.cs`

- `StandbyLine(int sec)` → `$"STBY:{Math.Clamp(sec, 5, 60)}\n"`.
- `QueryStandbyLine()` → `"STBY?\n"`.
- `TryParseStandby(string? line, out int value)` → parst `STBY:<n>`, clamp 5..60.

### `SerialLink.cs`

- `int? QueryStandby(int timeoutMs)` analog `QueryBrightness` (Write unter `_gate`,
  Read danach bis `STBY:n` oder Timeout).

### `TrayAppContext.cs`

- Untermenü **„Standby"** mit 12 Einträgen (5, 10, …, 60), Label `"<n> s"`:
  - **Fett** bei **5, 15, 30, 60**, sonst normal.
  - Haken am aktiven Wert.
- Beim Connect (Background-`Task`): `STBY?` abfragen → Wert cachen → Haken setzen.
- Klick auf eine Stufe: `STBY:n` senden, Cache + Haken aktualisieren.
- Submenu nur aktiv, wenn verbunden.

## Tests

### C# (xUnit)

- `StandbyLine`/`TryParseStandby` Roundtrip + Clamping (0, 4, 5, 60, 90).
- Ungültige/leere Zeilen → kein Parse; `STBY?` selbst ist keine Antwort.

### Firmware-Pure (on-device, deferred)

- `protocolParseLine` für `STBY:<n>` (Clamp) und `STBY?`.

### Hardware-E2E (nach „go" + freiem Gerät)

1. Tray → Standby → Stufe wählen → neuer Timeout greift (Display geht nach n s aus).
2. Board-Reboot → Wert bleibt (NVS).
3. Connect → Haken steht auf echtem Board-Wert (`STBY?`).

## Bewusst NICHT im Scope (YAGNI)

- Werte unter 5 s oder „Standby aus".
- Sekunden-genaue Feineinstellung (nur 5-s-Raster).
- Separate Timeouts für unterschiedliche Auslöser.
