# Tray-Helligkeitssteuerung — Design

**Datum:** 2026-06-15
**Branch (Worktree):** `worktree-tray-brightness`
**Status:** Freigegeben (User), bereit für Implementierungsplan

## Ziel

Über die Companion-Tray-App soll die **Gesamthelligkeit des Boards** (Display-Backlight,
GPIO46) eingestellt werden können — aber **nicht ganz aus**, denn dafür gibt es bereits den
Standby. Die Helligkeit wird **board-seitig im NVS-Flash** persistiert (Quelle der Wahrheit),
damit sie auch ohne laufende App über Reboots erhalten bleibt (Graceful Degradation).

## Verhalten / Wertebereich

- Helligkeit **5 %–100 %** in **5 %-Schritten** (20 Stufen). **5 % ist das Minimum** —
  „aus" bleibt dem Standby vorbehalten.
- Default auf jungfräulichem Board (NVS leer/unbeschrieben): **100 %**.
- Mapping Prozent → PWM: **linear** auf 8-bit-Duty: `duty = round(pct / 100 × 255)`
  (5 % → 13, 100 % → 255). Bewusst einfach; eine Gamma-/Perzeptiv-Kurve ist späteres
  Optional, kein Teil dieses Specs (YAGNI).

## Serial-Protokoll (Erweiterung)

Zeilenbasiert, ASCII, `\n`-terminiert — konsistent mit bestehenden Befehlen.

```
PC → Board:   BRIGHT:<5-100>\n    setzen + in NVS speichern
PC → Board:   BRIGHT?\n           aktuellen Wert abfragen
Board → PC:   BRIGHT:<n>\n         Antwort auf BRIGHT?
```

- Clamping: `BRIGHT:` wird auf **5..100** geklemmt (anders als `VOL:` mit 0..100, weil 0 = aus
  hier unerwünscht ist).
- Unbekannte/leere Zeilen weiterhin ignorieren; `\r\n` tolerieren; Overflow-Zeilen verwerfen.

## Firmware

### `protocol.h` (pure Parser)

- Neue Enum-Werte `ProtoCmd::SetBrightness` und `ProtoCmd::QueryBrightness`.
- `protocolParseLine()` erkennt:
  - `BRIGHT:` → `SetBrightness`, `value` auf **5..100** geklemmt.
  - `BRIGHT?` → `QueryBrightness`.
- Reihenfolge der `strncmp`-Checks beachten: `BRIGHT?` vor/getrennt von `BRIGHT:` (bzw. via
  Präfix `BRIGHT` + Folgezeichen unterscheiden), damit beide sauber matchen.
- Pure und damit on-device verifizierbar (kein Host-gcc auf dieser Maschine).

### `brightness.h/.cpp` (neues Modul)

- Hält den aktuellen Helligkeitswert (Prozent) im RAM.
- Kapselt **NVS via `Preferences`** (ESP32-Arduino): Namespace `audioknubbel`, Key `bright`.
  - `brightnessLoad()` → liest NVS, Default 100 wenn nicht gesetzt.
  - `brightnessSet(pct)` → clamp 5..100, anwenden (Backlight), in NVS schreiben.
  - `brightnessGet()` → aktueller Wert (für `BRIGHT?`-Antwort und Wake).
- Prozent→Duty-Mapping (linear) liegt hier (pure, testbar).

### `display.cpp`

- Neue Funktion `displayBacklightSetLevel(uint8_t pct)`: schreibt **nur den Duty-Wert** über die
  bestehende `ledcWrite(PIN_BL, duty)`.
- `displayBacklightSet(bool on)` bleibt für Standby erhalten:
  - `on`  → aktueller Helligkeits-Level (statt hart 255),
  - `off` → 0.
- **Tabu beachtet:** `ledcAttach`-Parameter (5 kHz / 8 bit), Power-Rails GPIO1/2/40 und der
  gesamte Init-Ablauf (`displayInit`) bleiben **unangetastet**. Geändert wird ausschließlich der
  **geschriebene Duty-Wert** — derselbe Mechanismus, der schon zwischen 255 und 0 wechselt.

### `main.cpp`

- `setup()`: nach `displayInit()` den NVS-Wert via `brightnessLoad()` anwenden. Kurzer
  Helligkeits-Sprung beim Boot (100 → gespeichert, < 1 s) ist akzeptabel.
- **Standby/Wake:** `wakeUp()` stellt über `displayBacklightSet(true)` wieder den gespeicherten
  Helligkeitswert her (nicht hart 255). Standby schaltet weiterhin auf 0.

### `protocol.cpp`

- `BRIGHT:` (`SetBrightness`) → `brightnessSet(value)` (anwenden + NVS speichern).
- `BRIGHT?` (`QueryBrightness`) → `USBSerial.println("BRIGHT:" + brightnessGet())`.
- Wie bei `ID?` zählt eine empfangene Zeile als Lebenszeichen (bestehende Logik greift schon).

## Companion-App

### `Protocol.cs`

- `BrightnessLine(int v)` → `$"BRIGHT:{Math.Clamp(v, 5, 100)}\n"`.
- `QueryBrightnessLine()` → `"BRIGHT?\n"`.
- `TryParseBrightness(string? line, out int value)` → parst `BRIGHT:<n>`-Antwort, clamp 5..100.

### `SerialLink.cs`

- `int? QueryBrightness(int timeoutMs)` analog zu `RequestBootCountdown`:
  Port unter Lock holen, `BRIGHT?` schreiben, Antwortzeilen bis `BRIGHT:n` oder Timeout lesen.
  Liest nur connect-/menü-nah; Heartbeat schreibt nur (kein Reader-Konflikt), Reconnect läuft
  nur bei getrennter Verbindung.

### `TrayAppContext.cs`

- Untermenü **„Helligkeit"** mit 20 Einträgen (5, 10, …, 100):
  - **Fett** (`FontStyle.Bold`) bei **5, 25, 50, 75, 100**, sonst normal.
  - Haken (`Checked`) am aktiven Wert.
- **Beim Connect** (Background-`Task`, nicht UI-Thread blockieren): `BRIGHT?` abfragen → Wert in
  Feld `_brightness` cachen → Haken setzen (Post auf UI-Thread). Da nur die App die Helligkeit
  ändert, bleibt der Cache danach korrekt.
- Klick auf eine Stufe: `BRIGHT:n` senden, `_brightness` + Haken aktualisieren.
- Bei getrenntem Board: Untermenü deaktiviert oder Klicks No-op (Send ist ohnehin No-op bei
  geschlossenem Port).

## Tests

### C# (xUnit — laufen auf dieser Maschine, dotnet 10)

- `Protocol`-Roundtrip: `BrightnessLine`/`TryParseBrightness`.
- Clamping auf 5..100 (Eingaben 0, 4, 5, 100, 150).
- Ungültige/leere Zeilen → kein Parse.

### Firmware-Pure (on-device verifiziert, kein Host-gcc)

- `protocolParseLine` für `BRIGHT:<n>` (inkl. Clamp 5..100) und `BRIGHT?`.

### Hardware-E2E (nach explizitem „go" zum Flashen)

1. Tray → Helligkeit → Stufe wählen → Board dimmt sichtbar.
2. Board-Reboot (Power-Cycle) → Helligkeit bleibt erhalten (NVS).
3. Standby (15 s Inaktivität) → Wake → kehrt zur eingestellten Helligkeit zurück (nicht 100 %).
4. Tray-Menü öffnen nach Connect → Haken steht auf dem echten Board-Wert.

## Risiken / Stolpersteine

- **GPIO46-Tabu (CLAUDE.md):** Bewusst nur der Duty-Wert wird geändert; Init/Attach/Power-Rails
  bleiben unberührt. Falls das Panel dennoch zickt → Flash zurückrollen, Init-Pfad prüfen.
- **NVS-Schreibzyklen:** Nutzer-Klicks sind selten; kein Debounce nötig. Falls später häufige
  programmatische Änderungen dazukommen, Schreib-Coalescing nachrüsten.
- **Port-Reader-Konflikt:** `QueryBrightness` liest den Port; sicher, weil der Heartbeat nur
  schreibt und Reconnect nur bei getrennter Verbindung aktiv ist.

## Bewusst NICHT im Scope (YAGNI)

- Gamma-/Perzeptiv-Helligkeitskurve.
- Helligkeit unter 5 % oder „aus" (= Standby).
- LED-Ring-Helligkeit (separates Thema).
- Automatik (Tageszeit/Umgebungslicht).
