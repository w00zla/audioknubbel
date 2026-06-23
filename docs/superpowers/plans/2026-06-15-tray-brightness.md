# Tray-Helligkeitssteuerung Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Die Gesamthelligkeit des Boards (Display-Backlight) über ein Tray-Untermenü in 5%-Schritten (5–100%) einstellen, board-seitig im NVS persistiert.

**Architecture:** Board ist Quelle der Wahrheit (NVS-Flash). Neue zeilenbasierte Serial-Befehle `BRIGHT:<n>` (setzen+speichern) und `BRIGHT?` (abfragen → Board antwortet `BRIGHT:n`). Firmware mappt Prozent linear auf 8-bit-PWM-Duty und stellt die Helligkeit nach Boot/Wake wieder her. Die Companion-App fragt beim Connect den aktiven Wert ab und zeigt ihn im Untermenü mit Haken.

**Tech Stack:** ESP32-S3 / Arduino-Core (PlatformIO), `Preferences` (NVS), LEDC-PWM (GPIO46); .NET 10 WinForms Companion, xUnit.

> **⚠️ Geräte-Beschränkung für diesen Lauf:** Es läuft parallel etwas auf dem Board/der App.
> **NICHT flashen, KEINEN Serial-Monitor öffnen, KEINEN COM-Port belegen, die laufende App
> NICHT starten.** Verifikation = C#-Tests + Firmware-Build + C#-Build. Hardware-E2E und
> On-Device-Parser-Check sind **deferred**, bis der User das Gerät freigibt (siehe Schluss).

---

## Datei-Struktur

| Datei | Verantwortung | Aktion |
|---|---|---|
| `src/protocol.h` | Pure Parser: `BRIGHT:` / `BRIGHT?` erkennen + clampen | Modify |
| `src/brightness.h` | Pure Mapping pct→duty (inline) + NVS-API-Deklaration | Create |
| `src/brightness.cpp` | NVS-Load/Set/Get via `Preferences`, Backlight anwenden | Create |
| `src/display.h` | Deklaration `displayBacklightSetLevel` | Modify |
| `src/display.cpp` | Duty-basiertes Backlight; Standby-On restauriert Level | Modify |
| `src/protocol.cpp` | `BRIGHT:`/`BRIGHT?`-Handler | Modify |
| `src/main.cpp` | `brightnessInit()` im setup, `protocolApplyBrightness` | Modify |
| `companion/AudioKnubbel.Companion/Protocol.cs` | `BrightnessLine`/`QueryBrightnessLine`/`TryParseBrightness` | Modify |
| `companion/AudioKnubbel.Companion/SerialLink.cs` | `QueryBrightness(timeoutMs)` | Modify |
| `companion/AudioKnubbel.Companion/TrayAppContext.cs` | Untermenü „Helligkeit" + Connect-Query | Modify |
| `companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs` | Tests für die neuen Protocol-Methoden | Modify |

**Build-/Testbefehle (Referenz):**
- Firmware-Build: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
- C#-Tests: `dotnet test companion/AudioKnubbel.Companion.Tests`
- C#-Build: `dotnet build companion/AudioKnubbel.Companion`

---

## Task 1: C# Protocol — Brightness-Zeilen (TDD)

**Files:**
- Modify: `companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs`
- Modify: `companion/AudioKnubbel.Companion/Protocol.cs`

- [ ] **Step 1: Failing-Tests schreiben**

Am Ende von `ProtocolTests.cs` (vor der schließenden `}` der Klasse) einfügen:

```csharp
    [Theory]
    [InlineData(55, "BRIGHT:55\n")]
    [InlineData(5, "BRIGHT:5\n")]
    [InlineData(100, "BRIGHT:100\n")]
    [InlineData(0, "BRIGHT:5\n")]      // unter Minimum -> 5
    [InlineData(4, "BRIGHT:5\n")]      // unter Minimum -> 5
    [InlineData(150, "BRIGHT:100\n")]  // über Maximum -> 100
    [InlineData(-10, "BRIGHT:5\n")]
    public void BrightnessLine_ClampsAndFormats(int input, string expected)
        => Assert.Equal(expected, Protocol.BrightnessLine(input));

    [Fact]
    public void QueryBrightnessLine_IsNewlineTerminated()
        => Assert.Equal("BRIGHT?\n", Protocol.QueryBrightnessLine());

    [Theory]
    [InlineData("BRIGHT:55", true, 55)]
    [InlineData("BRIGHT:5\r", true, 5)]
    [InlineData("  BRIGHT:100", true, 100)]   // führende Whitespaces tolerieren
    [InlineData("BRIGHT:0", true, 5)]         // Antwort unter Minimum -> 5
    [InlineData("BRIGHT:150", true, 100)]     // Antwort über Maximum -> 100
    [InlineData("BRIGHT?", false, 0)]         // das Query-Kommando ist keine Antwort
    [InlineData("VOL:55", false, 0)]
    [InlineData("", false, 0)]
    [InlineData(null, false, 0)]
    public void TryParseBrightness_ParsesReply(string? line, bool ok, int expected)
    {
        bool result = Protocol.TryParseBrightness(line, out int value);
        Assert.Equal(ok, result);
        if (ok) Assert.Equal(expected, value);
    }
```

- [ ] **Step 2: Test ausführen, Fehlschlag verifizieren**

Run: `dotnet test companion/AudioKnubbel.Companion.Tests`
Expected: FAIL (Kompilierfehler — `Protocol.BrightnessLine`/`QueryBrightnessLine`/`TryParseBrightness` existieren nicht).

- [ ] **Step 3: Implementierung in `Protocol.cs`**

In `Protocol.cs` vor der schließenden `}` der Klasse einfügen:

```csharp
    // Helligkeit (Backlight): App setzt 5..100; 0/aus ist dem Standby vorbehalten.
    public static string BrightnessLine(int v) => $"BRIGHT:{Math.Clamp(v, 5, 100)}\n";
    public static string QueryBrightnessLine() => "BRIGHT?\n";

    // Parst die Board-Antwort "BRIGHT:<n>" (auf BRIGHT?); clamp auf 5..100.
    // "BRIGHT?" selbst und andere Zeilen ergeben false.
    public static bool TryParseBrightness(string? line, out int value)
    {
        value = 0;
        if (line is null) return false;
        var s = line.Trim();
        const string prefix = "BRIGHT:";
        if (!s.StartsWith(prefix, StringComparison.Ordinal)) return false;
        if (!int.TryParse(s.AsSpan(prefix.Length), out int v)) return false;
        value = Math.Clamp(v, 5, 100);
        return true;
    }
```

- [ ] **Step 4: Test ausführen, Erfolg verifizieren**

Run: `dotnet test companion/AudioKnubbel.Companion.Tests`
Expected: PASS (alle Tests grün, inkl. der bestehenden).

- [ ] **Step 5: Commit**

```bash
git add companion/AudioKnubbel.Companion/Protocol.cs companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs
git commit -m "feat(companion): brightness protocol lines + parser"
```

---

## Task 2: Firmware Pure-Parser — `BRIGHT:` / `BRIGHT?`

**Files:**
- Modify: `src/protocol.h`

- [ ] **Step 1: Enum erweitern**

In `src/protocol.h` die Enum-Zeile ersetzen:

```cpp
enum class ProtoCmd : uint8_t { None, SetVolume, SetMute, Identify, Ping, EnterBoot, SetBrightness, QueryBrightness };
```

- [ ] **Step 2: Parser-Zweige ergänzen**

In `protocolParseLine()` **vor** dem `else if (strncmp(line, "ID?", 3) == 0)`-Zweig einfügen
(Reihenfolge: `BRIGHT?` vor `BRIGHT:` prüfen, damit das Query-Kommando nicht als Set matcht):

```cpp
    } else if (strncmp(line, "BRIGHT?", 7) == 0) {
        r.cmd = ProtoCmd::QueryBrightness;
    } else if (strncmp(line, "BRIGHT:", 7) == 0) {
        int v = atoi(line + 7);
        if (v < 5)   v = 5;     // 5 % ist Minimum — „aus" macht der Standby
        if (v > 100) v = 100;
        r.cmd = ProtoCmd::SetBrightness;
        r.value = v;
```

So liest der Block danach weiter mit `} else if (strncmp(line, "ID?", 3) == 0) {`.

- [ ] **Step 3: Firmware-Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS` (kompiliert; `protocol.cpp` nutzt die neuen Enum-Werte erst in Task 5 — der `switch` dort ist noch ohne diese Cases, das ist ok, nur eine Warnung möglich).

Hinweis: On-Device-Verifikation des Parsers ist **deferred** (kein Flashen in diesem Lauf).

- [ ] **Step 4: Commit**

```bash
git add src/protocol.h
git commit -m "feat(fw): parse BRIGHT set/query in pure protocol parser"
```

---

## Task 3: Firmware Display — Duty-basiertes Backlight

**Files:**
- Modify: `src/display.h`
- Modify: `src/display.cpp`

- [ ] **Step 1: Deklaration in `display.h`**

Nach der Zeile `void displayBacklightSet(bool on);` einfügen:

```cpp

// Backlight-Helligkeit per 8-bit-Duty (0..255) setzen. Merkt sich den Wert, damit
// displayBacklightSet(true) (Wake aus Standby) ihn wiederherstellt. PWM auf GPIO46.
void displayBacklightSetLevel(uint8_t duty);
```

- [ ] **Step 2: `display.cpp` umbauen**

Den bestehenden Block in `src/display.cpp` …

```cpp
void displayBacklightSet(bool on) {
    static bool attached = false;
    if (!attached) {
        ledcAttach(PIN_BL, 5000, 8);
        attached = true;
    }
    ledcWrite(PIN_BL, on ? 255 : 0);
}
```

… vollständig ersetzen durch (LEDC-Attach-Parameter 5 kHz/8 bit bleiben **unverändert** —
geändert wird nur der geschriebene Duty-Wert):

```cpp
static uint8_t s_bl_duty = 255;   // zuletzt gesetzte Helligkeit (für Wake aus Standby)

static void blEnsureAttached() {
    static bool attached = false;
    if (!attached) {
        ledcAttach(PIN_BL, 5000, 8);
        attached = true;
    }
}

void displayBacklightSetLevel(uint8_t duty) {
    s_bl_duty = duty;
    blEnsureAttached();
    ledcWrite(PIN_BL, duty);
}

void displayBacklightSet(bool on) {
    blEnsureAttached();
    ledcWrite(PIN_BL, on ? s_bl_duty : 0);
}
```

- [ ] **Step 3: Firmware-Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/display.h src/display.cpp
git commit -m "feat(fw): duty-based backlight, standby restores last level"
```

---

## Task 4: Firmware Brightness-Modul (NVS + Mapping)

**Files:**
- Create: `src/brightness.h`
- Create: `src/brightness.cpp`

- [ ] **Step 1: `src/brightness.h` anlegen**

```cpp
#pragma once
#include <stdint.h>

// Pure: Helligkeit in Prozent (wird auf 5..100 geklemmt) auf 8-bit-PWM-Duty
// abbilden. Linear: 5 % -> 13, 100 % -> 255. Header-only & ohne Arduino, damit
// on-device/pur nachvollziehbar (wie encoderQuadStep/protocolParseLine).
inline uint8_t brightnessPctToDuty(int pct) {
    if (pct < 5)   pct = 5;
    if (pct > 100) pct = 100;
    return (uint8_t)((pct * 255 + 50) / 100);
}

#ifdef ARDUINO
// NVS-gestützte Helligkeit. brightnessInit() lädt den Wert (Default 100) und
// wendet ihn aufs Backlight an. brightnessSet() clamped, wendet an, speichert.
void brightnessInit();
void brightnessSet(int pct);
int  brightnessGet();
#endif
```

- [ ] **Step 2: `src/brightness.cpp` anlegen**

```cpp
#include "brightness.h"
#ifdef ARDUINO
#include <Preferences.h>
#include "display.h"

static Preferences s_prefs;
static int         s_brightness = 100;

void brightnessInit() {
    s_prefs.begin("audioknubbel", false);            // NVS-Namespace, read/write
    s_brightness = s_prefs.getInt("bright", 100); // Default 100 % auf frischem Board
    if (s_brightness < 5)   s_brightness = 5;
    if (s_brightness > 100) s_brightness = 100;
    displayBacklightSetLevel(brightnessPctToDuty(s_brightness));
}

void brightnessSet(int pct) {
    if (pct < 5)   pct = 5;
    if (pct > 100) pct = 100;
    s_brightness = pct;
    displayBacklightSetLevel(brightnessPctToDuty(pct));
    s_prefs.putInt("bright", pct);
}

int brightnessGet() { return s_brightness; }
#endif
```

- [ ] **Step 3: Firmware-Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS` (Modul wird noch nicht aufgerufen — nur Kompilierbarkeit prüfen).

- [ ] **Step 4: Commit**

```bash
git add src/brightness.h src/brightness.cpp
git commit -m "feat(fw): brightness module with NVS persistence + pct->duty mapping"
```

---

## Task 5: Firmware Verdrahtung — protocol.cpp + main.cpp

**Files:**
- Modify: `src/protocol.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: `main.cpp` — Handler + Init**

In `src/main.cpp` `#include "touch.h"` ergänzen um:

```cpp
#include "brightness.h"
```

Nach der Funktion `protocolApplyMute(...)` (vor `protocolEnterBoot`) einfügen:

```cpp
// Vom Serial-Parser auf "BRIGHT:<n>" aufgerufen: Helligkeit setzen + persistieren.
// Weckt das Board, damit die Änderung sichtbar wird.
void protocolApplyBrightness(int pct) {
    brightnessSet(pct);
    wakeUp();
}
```

In `setup()` direkt **nach** `displayInit();` einfügen:

```cpp
    brightnessInit();                // Helligkeit aus NVS laden + aufs Backlight anwenden
```

- [ ] **Step 2: `protocol.cpp` — Befehle behandeln**

In `src/protocol.cpp` nach `#include "hid.h"   // USBSerial` ergänzen:

```cpp
#include "brightness.h"   // brightnessGet() für die BRIGHT?-Antwort
```

Die `extern void protocolApplyMute(...)`-Gruppe um eine Deklaration erweitern:

```cpp
extern void protocolApplyBrightness(int pct);   // in main.cpp: setzen + speichern + wecken
```

Im `switch (r.cmd)` in `handleLine()` die beiden neuen Cases ergänzen (vor `case ProtoCmd::None:`):

```cpp
        case ProtoCmd::SetBrightness:   protocolApplyBrightness(r.value);     break;
        case ProtoCmd::QueryBrightness: {
            char buf[16];
            snprintf(buf, sizeof(buf), "BRIGHT:%d", brightnessGet());
            USBSerial.println(buf);
            break;
        }
```

- [ ] **Step 3: Firmware-Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS` (alle `ProtoCmd`-Cases abgedeckt, keine Switch-Warnung mehr).

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp src/protocol.cpp
git commit -m "feat(fw): wire brightness set/query into protocol + boot load"
```

---

## Task 6: Companion SerialLink — `QueryBrightness`

**Files:**
- Modify: `companion/AudioKnubbel.Companion/SerialLink.cs`

- [ ] **Step 1: Methode hinzufügen**

In `SerialLink.cs` nach der Methode `RequestBootCountdown(...)` einfügen:

```csharp
    // Fragt die aktuelle Board-Helligkeit ab ("BRIGHT?") und wartet bis timeoutMs
    // auf die "BRIGHT:n"-Antwort. Gibt den Wert (5..100) zurück oder null. Der
    // Schreibvorgang läuft unter _gate (kein Byte-Interleave mit dem Heartbeat-PING),
    // das Lesen danach außerhalb (einziger Reader im Normalbetrieb).
    public int? QueryBrightness(int timeoutMs)
    {
        SerialPort? p;
        lock (_gate)
        {
            p = _port;
            if (p?.IsOpen != true) return null;
            try { p.DiscardInBuffer(); p.Write(Protocol.QueryBrightnessLine()); }
            catch { return null; }
        }
        try
        {
            var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
            while (DateTime.UtcNow < deadline)
            {
                int remaining = (int)(deadline - DateTime.UtcNow).TotalMilliseconds;
                p.ReadTimeout = Math.Clamp(remaining, 50, timeoutMs);
                try
                {
                    if (Protocol.TryParseBrightness(p.ReadLine(), out int v)) return v;
                }
                catch (TimeoutException) { break; }
            }
        }
        catch { /* Port-Fehler -> kein Wert */ }
        return null;
    }
```

- [ ] **Step 2: Build verifizieren**

Run: `dotnet build companion/AudioKnubbel.Companion`
Expected: `Build succeeded`.

- [ ] **Step 3: Commit**

```bash
git add companion/AudioKnubbel.Companion/SerialLink.cs
git commit -m "feat(companion): SerialLink.QueryBrightness"
```

---

## Task 7: Companion Tray — Helligkeits-Untermenü

**Files:**
- Modify: `companion/AudioKnubbel.Companion/TrayAppContext.cs`

- [ ] **Step 1: Feld hinzufügen**

In `TrayAppContext.cs` nach der Zeile `private ToolStripMenuItem _statusItem = null!;` einfügen:

```csharp
    private ToolStripMenuItem _brightnessMenu = null!;   // Untermenü „Helligkeit"
    private int _brightness = 100;                        // gecachter Board-Wert (Haken)
```

- [ ] **Step 2: Untermenü bauen + einhängen**

In `BuildMenu()` nach `menu.Items.Add(autostart);` und **vor**
`menu.Items.Add(new ToolStripSeparator());` (dem Separator vor dem Flash-Eintrag) einfügen:

```csharp
        _brightnessMenu = BuildBrightnessMenu();
        menu.Items.Add(_brightnessMenu);
```

Im bestehenden `menu.Opening += (_, _) => { ... }`-Handler die zwei Zeilen ergänzen
(nach `UpdateStatus();`):

```csharp
            _brightnessMenu.Enabled = _link.Connected;
            UpdateBrightnessChecks();
```

Neue Methoden in der Klasse hinzufügen (z. B. nach `BuildMenu()`):

```csharp
    // Untermenü „Helligkeit": 5..100 % in 5er-Schritten; 5/25/50/75/100 fett.
    private ToolStripMenuItem BuildBrightnessMenu()
    {
        var root = new ToolStripMenuItem("Helligkeit");
        for (int pct = 5; pct <= 100; pct += 5)
        {
            int value = pct;
            var item = new ToolStripMenuItem($"{pct}%") { Tag = value };
            if (pct == 5 || pct == 25 || pct == 50 || pct == 75 || pct == 100)
                item.Font = new Font(item.Font, FontStyle.Bold);
            item.Click += (_, _) => SetBrightness(value);
            root.DropDownItems.Add(item);
        }
        return root;
    }

    // Setzt die Helligkeit am Board (BRIGHT:n) und spiegelt den Haken.
    private void SetBrightness(int pct)
    {
        _brightness = pct;
        _link.Send(Protocol.BrightnessLine(pct));
        UpdateBrightnessChecks();
    }

    // Haken auf den gecachten Wert setzen.
    private void UpdateBrightnessChecks()
    {
        foreach (ToolStripMenuItem item in _brightnessMenu.DropDownItems)
            item.Checked = (int)item.Tag! == _brightness;
    }

    // Beim Connect den echten Board-Wert abfragen (Background-Thread, kein UI-Block)
    // und Haken aktualisieren. Board ist Quelle der Wahrheit (NVS).
    private void QueryBrightnessAsync()
    {
        System.Threading.Tasks.Task.Run(() =>
        {
            int? v = _link.QueryBrightness(700);
            if (v is int b) Post(() => { _brightness = b; UpdateBrightnessChecks(); });
        });
    }
```

- [ ] **Step 3: Connect-Hook erweitern**

Im `OnConnectionChanged`-Handler nach `_sync.Sync(_monitor.Current);` einfügen:

```csharp
            QueryBrightnessAsync();
```

- [ ] **Step 4: Build verifizieren**

Run: `dotnet build companion/AudioKnubbel.Companion`
Expected: `Build succeeded`.

- [ ] **Step 5: Commit**

```bash
git add companion/AudioKnubbel.Companion/TrayAppContext.cs
git commit -m "feat(companion): brightness submenu with board query + checkmarks"
```

---

## Task 8: Gesamtverifikation (ohne Gerät)

**Files:** keine Änderung — nur Verifikation.

- [ ] **Step 1: C#-Tests**

Run: `dotnet test companion/AudioKnubbel.Companion.Tests`
Expected: PASS (alle Tests grün).

- [ ] **Step 2: C#-Build (App)**

Run: `dotnet build companion/AudioKnubbel.Companion`
Expected: `Build succeeded`, 0 Errors.

- [ ] **Step 3: Firmware-Build**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS`.

- [ ] **Step 4: Diff sichten**

Run: `git log --oneline main..HEAD` und `git diff --stat main..HEAD`
Expected: Commits aus Tasks 1–7, nur die geplanten Dateien geändert.

---

## Deferred — Hardware-Verifikation (erst nach „go" + freiem Gerät)

Diese Schritte **nicht** in diesem Lauf ausführen (Gerät/App sind parallel belegt). Nach
explizitem „go" des Users, wenn der COM-Port frei ist:

1. Firmware flashen (`pio.exe run -e crowpanel-s3 -t upload`, Board im Flash-Mode, COM5).
2. On-Device-Parser-Check: `BRIGHT:50` / `BRIGHT?` über Serial — Board dimmt / antwortet `BRIGHT:50`.
3. Tray → Helligkeit → Stufe wählen → Board dimmt sichtbar; Haken sitzt richtig.
4. Board-Reboot (Power-Cycle) → Helligkeit bleibt erhalten (NVS).
5. Standby (15 s) → Wake → kehrt zur eingestellten Helligkeit zurück (nicht 100 %).
6. Tray-Menü nach Connect öffnen → Haken steht auf dem echten Board-Wert (BRIGHT?-Query).

---

## Self-Review-Notiz

- **Spec-Abdeckung:** Wertebereich/Schritte (Task 7), Default 100 (Task 4), Mapping (Task 4),
  Protokoll `BRIGHT:`/`BRIGHT?` (Tasks 1,2,5), NVS (Task 4), Backlight-Duty + Wake-Restore
  (Task 3), App-Query/Haken/Bold (Tasks 6,7), Tests (Tasks 1,8). Alle Spec-Punkte abgedeckt.
- **Tabu beachtet:** Nur Duty-Wert in `display.cpp` geändert; `ledcAttach`, Power-Rails,
  `displayInit`-Ablauf unangetastet.
- **Typ-Konsistenz:** `brightnessPctToDuty`/`brightnessSet`/`brightnessGet`,
  `displayBacklightSetLevel`, `protocolApplyBrightness`, `Protocol.BrightnessLine`/
  `QueryBrightnessLine`/`TryParseBrightness`, `SerialLink.QueryBrightness` — Namen über alle
  Tasks identisch verwendet.
