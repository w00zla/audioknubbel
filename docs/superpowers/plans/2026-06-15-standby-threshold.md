# Standby-Threshold übers Tray Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Den Standby-Timeout des Boards über ein Tray-Untermenü in 5-s-Schritten (5–60 s) einstellen, board-seitig im NVS persistiert.

**Architecture:** Board ist Quelle der Wahrheit (NVS-Flash). Neue Serial-Befehle `STBY:<n>` (setzen+speichern) und `STBY?` (abfragen → `STBY:n`). `main.cpp` nutzt den NVS-Wert (Sekunden × 1000) als Standby-Schwelle statt eines Hardcodes. Die App fragt beim Connect den aktiven Wert ab und zeigt ihn im Untermenü mit Haken. Spiegelt das Helligkeits-Feature.

**Tech Stack:** ESP32-S3 / Arduino-Core (PlatformIO), `Preferences` (NVS); .NET 10 WinForms Companion, xUnit.

> **⚠️ Geräte-Beschränkung:** **NICHT flashen, KEINEN Serial-Monitor öffnen, KEINEN COM-Port
> belegen** — der User flasht selbst. Verifikation = C#-Tests + Firmware-Build + C#-Build.
> Hardware-E2E ist deferred.

---

## Datei-Struktur

| Datei | Verantwortung | Aktion |
|---|---|---|
| `src/protocol.h` | Pure Parser: `STBY:` / `STBY?` erkennen + clampen | Modify |
| `src/standby_cfg.h` | NVS-API-Deklaration (Sekunden) | Create |
| `src/standby_cfg.cpp` | NVS-Load/Set/Get via `Preferences` | Create |
| `src/protocol.cpp` | `STBY:`/`STBY?`-Handler | Modify |
| `src/main.cpp` | `standbyCfgInit()`, `protocolApplyStandby`, Check nutzt Getter | Modify |
| `companion/AudioKnubbel.Companion/Protocol.cs` | `StandbyLine`/`QueryStandbyLine`/`TryParseStandby` | Modify |
| `companion/AudioKnubbel.Companion/SerialLink.cs` | `QueryStandby(timeoutMs)` | Modify |
| `companion/AudioKnubbel.Companion/TrayAppContext.cs` | Untermenü „Standby" + Connect-Query | Modify |
| `companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs` | Tests für die neuen Protocol-Methoden | Modify |

**Build-/Testbefehle:**
- Firmware-Build: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
- C#-Tests: `dotnet test companion/AudioKnubbel.Companion.Tests`
- C#-Build: `dotnet build companion/AudioKnubbel.Companion`

---

## Task 1: C# Protocol — Standby-Zeilen (TDD)

**Files:**
- Modify: `companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs`
- Modify: `companion/AudioKnubbel.Companion/Protocol.cs`

- [ ] **Step 1: Failing-Tests schreiben**

Am Ende von `ProtocolTests.cs` (vor der schließenden `}` der Klasse — direkt nach dem
`TryParseBrightness_ParsesReply`-Test) einfügen:

```csharp
    [Theory]
    [InlineData(15, "STBY:15\n")]
    [InlineData(5, "STBY:5\n")]
    [InlineData(60, "STBY:60\n")]
    [InlineData(0, "STBY:5\n")]      // unter Minimum -> 5
    [InlineData(4, "STBY:5\n")]      // unter Minimum -> 5
    [InlineData(90, "STBY:60\n")]    // über Maximum -> 60
    [InlineData(-10, "STBY:5\n")]
    public void StandbyLine_ClampsAndFormats(int input, string expected)
        => Assert.Equal(expected, Protocol.StandbyLine(input));

    [Fact]
    public void QueryStandbyLine_IsNewlineTerminated()
        => Assert.Equal("STBY?\n", Protocol.QueryStandbyLine());

    [Theory]
    [InlineData("STBY:15", true, 15)]
    [InlineData("STBY:5\r", true, 5)]
    [InlineData("  STBY:60", true, 60)]      // führende Whitespaces tolerieren
    [InlineData("STBY:0", true, 5)]          // Antwort unter Minimum -> 5
    [InlineData("STBY:90", true, 60)]        // Antwort über Maximum -> 60
    [InlineData("STBY?", false, 0)]          // das Query-Kommando ist keine Antwort
    [InlineData("VOL:15", false, 0)]
    [InlineData("", false, 0)]
    [InlineData(null, false, 0)]
    public void TryParseStandby_ParsesReply(string? line, bool ok, int expected)
    {
        bool result = Protocol.TryParseStandby(line, out int value);
        Assert.Equal(ok, result);
        if (ok) Assert.Equal(expected, value);
    }
```

- [ ] **Step 2: Test ausführen, Fehlschlag verifizieren**

Run: `dotnet test companion/AudioKnubbel.Companion.Tests`
Expected: FAIL (Kompilierfehler — `Protocol.StandbyLine`/`QueryStandbyLine`/`TryParseStandby` fehlen).

- [ ] **Step 3: Implementierung in `Protocol.cs`**

In `Protocol.cs` vor der schließenden `}` der Klasse (nach `TryParseBrightness`) einfügen:

```csharp
    // Standby-Timeout in Sekunden: App setzt 5..60.
    public static string StandbyLine(int sec) => $"STBY:{Math.Clamp(sec, 5, 60)}\n";
    public static string QueryStandbyLine() => "STBY?\n";

    // Parst die Board-Antwort "STBY:<n>" (auf STBY?); clamp auf 5..60.
    // "STBY?" selbst und andere Zeilen ergeben false.
    public static bool TryParseStandby(string? line, out int value)
    {
        value = 0;
        if (line is null) return false;
        var s = line.Trim();
        const string prefix = "STBY:";
        if (!s.StartsWith(prefix, StringComparison.Ordinal)) return false;
        if (!int.TryParse(s.AsSpan(prefix.Length), out int v)) return false;
        value = Math.Clamp(v, 5, 60);
        return true;
    }
```

- [ ] **Step 4: Test ausführen, Erfolg verifizieren**

Run: `dotnet test companion/AudioKnubbel.Companion.Tests`
Expected: PASS (alle Tests grün).

- [ ] **Step 5: Commit**

```bash
git add companion/AudioKnubbel.Companion/Protocol.cs companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs
git commit -m "feat(companion): standby protocol lines + parser"
```

---

## Task 2: Firmware Pure-Parser — `STBY:` / `STBY?`

**Files:**
- Modify: `src/protocol.h`

- [ ] **Step 1: Enum erweitern**

In `src/protocol.h` die Enum-Zeile ersetzen:

```cpp
enum class ProtoCmd : uint8_t { None, SetVolume, SetMute, Identify, Ping, EnterBoot, SetBrightness, QueryBrightness, SetStandby, QueryStandby };
```

- [ ] **Step 2: Parser-Zweige ergänzen**

In `protocolParseLine()` **vor** dem `} else if (strncmp(line, "ID?", 3) == 0) {`-Zweig einfügen
(Reihenfolge: `STBY?` vor `STBY:`):

```cpp
    } else if (strncmp(line, "STBY?", 5) == 0) {
        r.cmd = ProtoCmd::QueryStandby;
    } else if (strncmp(line, "STBY:", 5) == 0) {
        int v = atoi(line + 5);
        if (v < 5)  v = 5;
        if (v > 60) v = 60;
        r.cmd = ProtoCmd::SetStandby;
        r.value = v;
```

- [ ] **Step 3: Firmware-Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS` (die neuen Cases werden erst in Task 4 im `switch` behandelt — bis dahin
evtl. eine Switch-Warnung, kein Fehler).

- [ ] **Step 4: Commit**

```bash
git add src/protocol.h
git commit -m "feat(fw): parse STBY set/query in pure protocol parser"
```

---

## Task 3: Firmware Standby-Config-Modul (NVS)

**Files:**
- Create: `src/standby_cfg.h`
- Create: `src/standby_cfg.cpp`

- [ ] **Step 1: `src/standby_cfg.h` anlegen**

```cpp
#pragma once

#ifdef ARDUINO
// NVS-gestützter Standby-Timeout in Sekunden (5..60). standbyCfgInit() lädt den
// Wert (Default 15). standbyCfgSet() clamped + speichert. standbyCfgGet() liefert
// den aktuellen Wert (für STBY?-Antwort und den Standby-Check in main.cpp).
void standbyCfgInit();
void standbyCfgSet(int sec);
int  standbyCfgGet();
#endif
```

- [ ] **Step 2: `src/standby_cfg.cpp` anlegen**

```cpp
#include "standby_cfg.h"
#ifdef ARDUINO
#include <Preferences.h>

static Preferences s_prefs;
static int         s_standby = 15;

void standbyCfgInit() {
    s_prefs.begin("audioknubbel", false);             // NVS-Namespace, read/write
    s_standby = s_prefs.getInt("standby", 15);    // Default 15 s
    if (s_standby < 5)  s_standby = 5;
    if (s_standby > 60) s_standby = 60;
}

void standbyCfgSet(int sec) {
    if (sec < 5)  sec = 5;
    if (sec > 60) sec = 60;
    s_standby = sec;
    s_prefs.putInt("standby", sec);
}

int standbyCfgGet() { return s_standby; }
#endif
```

- [ ] **Step 3: Firmware-Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS` (Modul noch nicht aufgerufen — nur Kompilierbarkeit).

- [ ] **Step 4: Commit**

```bash
git add src/standby_cfg.h src/standby_cfg.cpp
git commit -m "feat(fw): standby-config module with NVS persistence"
```

---

## Task 4: Firmware Verdrahtung — protocol.cpp + main.cpp

**Files:**
- Modify: `src/protocol.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: `main.cpp` — Include + Handler + Init + Check**

In `src/main.cpp` nach `#include "brightness.h"` ergänzen:

```cpp
#include "standby_cfg.h"
```

Die Zeile

```cpp
static const uint32_t STANDBY_TIMEOUT_MS = 15000;
```

ersetzen durch (Kommentar anpassen — Wert kommt jetzt aus NVS):

```cpp
// Standby-Timeout wird aus NVS gelesen (standby_cfg, Sekunden) und unten in ms genutzt.
```

Nach der Funktion `protocolApplyBrightness(...)` einfügen:

```cpp
// Vom Serial-Parser auf "STBY:<n>" aufgerufen: Standby-Timeout (Sekunden) setzen +
// persistieren. wakeUp() setzt den Aktivitäts-Timer zurück, damit die neue Schwelle
// ab jetzt frisch zählt.
void protocolApplyStandby(int sec) {
    standbyCfgSet(sec);
    wakeUp();
}
```

In `setup()` direkt **nach** `brightnessInit();` einfügen:

```cpp
    standbyCfgInit();                // Standby-Timeout aus NVS laden
```

Den Standby-Check ersetzen — aus

```cpp
    if (!s_standby && (millis() - s_last_activity_ms >= STANDBY_TIMEOUT_MS)) {
```

wird

```cpp
    if (!s_standby && (millis() - s_last_activity_ms >= (uint32_t)standbyCfgGet() * 1000UL)) {
```

- [ ] **Step 2: `protocol.cpp` — Befehle behandeln**

In `src/protocol.cpp` nach `#include "brightness.h"   // brightnessGet() für die BRIGHT?-Antwort`
ergänzen:

```cpp
#include "standby_cfg.h"  // standbyCfgGet() für die STBY?-Antwort
```

Die `extern`-Gruppe um eine Deklaration erweitern (nach `protocolApplyBrightness`):

```cpp
extern void protocolApplyStandby(int sec);      // in main.cpp: setzen + speichern + wecken
```

Im `switch (r.cmd)` in `handleLine()` die beiden neuen Cases ergänzen (vor `case ProtoCmd::None:`):

```cpp
        case ProtoCmd::SetStandby:      protocolApplyStandby(r.value);        break;
        case ProtoCmd::QueryStandby: {
            char buf[16];
            snprintf(buf, sizeof(buf), "STBY:%d", standbyCfgGet());
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
git commit -m "feat(fw): wire standby set/query into protocol + NVS-driven timeout"
```

---

## Task 5: Companion SerialLink — `QueryStandby`

**Files:**
- Modify: `companion/AudioKnubbel.Companion/SerialLink.cs`

- [ ] **Step 1: Methode hinzufügen**

In `SerialLink.cs` nach der Methode `QueryBrightness(...)` einfügen:

```csharp
    // Fragt den aktuellen Standby-Timeout ab ("STBY?") und wartet bis timeoutMs auf
    // die "STBY:n"-Antwort. Gibt den Wert (5..60) zurück oder null. Schreibvorgang
    // unter _gate (kein Byte-Interleave mit dem Heartbeat-PING), Lesen danach.
    public int? QueryStandby(int timeoutMs)
    {
        SerialPort? p;
        lock (_gate)
        {
            p = _port;
            if (p?.IsOpen != true) return null;
            try { p.DiscardInBuffer(); p.Write(Protocol.QueryStandbyLine()); }
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
                    if (Protocol.TryParseStandby(p.ReadLine(), out int v)) return v;
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
git commit -m "feat(companion): SerialLink.QueryStandby"
```

---

## Task 6: Companion Tray — Standby-Untermenü

**Files:**
- Modify: `companion/AudioKnubbel.Companion/TrayAppContext.cs`

- [ ] **Step 1: Felder hinzufügen**

In `TrayAppContext.cs` nach der Zeile `private int _brightness = 100;` einfügen:

```csharp
    private ToolStripMenuItem _standbyMenu = null!;      // Untermenü „Standby"
    private int _standby = 15;                            // gecachter Board-Wert (Haken)
```

- [ ] **Step 2: Untermenü bauen + einhängen**

In `BuildMenu()` nach `menu.Items.Add(_brightnessMenu);` einfügen:

```csharp
        _standbyMenu = BuildStandbyMenu();
        menu.Items.Add(_standbyMenu);
```

Im `menu.Opening += (_, _) => { ... }`-Handler nach `UpdateBrightnessChecks();` ergänzen:

```csharp
            _standbyMenu.Enabled = _link.Connected;
            UpdateStandbyChecks();
```

Neue Methoden in der Klasse hinzufügen (z. B. nach `QueryBrightnessAsync()`):

```csharp
    // Untermenü „Standby": 5..60 s in 5er-Schritten; 5/15/30/60 fett.
    private ToolStripMenuItem BuildStandbyMenu()
    {
        var root = new ToolStripMenuItem("Standby");
        for (int sec = 5; sec <= 60; sec += 5)
        {
            int value = sec;
            var item = new ToolStripMenuItem($"{sec} s") { Tag = value };
            if (sec == 5 || sec == 15 || sec == 30 || sec == 60)
                item.Font = new Font(item.Font, FontStyle.Bold);
            item.Click += (_, _) => SetStandby(value);
            root.DropDownItems.Add(item);
        }
        return root;
    }

    // Setzt den Standby-Timeout am Board (STBY:n) und spiegelt den Haken.
    private void SetStandby(int sec)
    {
        _standby = sec;
        _link.Send(Protocol.StandbyLine(sec));
        UpdateStandbyChecks();
    }

    // Haken auf den gecachten Wert setzen.
    private void UpdateStandbyChecks()
    {
        foreach (ToolStripMenuItem item in _standbyMenu.DropDownItems)
            item.Checked = (int)item.Tag! == _standby;
    }

    // Beim Connect den echten Board-Wert abfragen (Background-Thread, kein UI-Block).
    private void QueryStandbyAsync()
    {
        System.Threading.Tasks.Task.Run(() =>
        {
            int? v = _link.QueryStandby(700);
            if (v is int s) Post(() => { _standby = s; UpdateStandbyChecks(); });
        });
    }
```

- [ ] **Step 3: Connect-Hook erweitern**

Im `OnConnectionChanged`-Handler nach `QueryBrightnessAsync();` einfügen:

```csharp
            QueryStandbyAsync();             // echten Standby-Wert vom Board holen
```

- [ ] **Step 4: Build verifizieren**

Run: `dotnet build companion/AudioKnubbel.Companion`
Expected: `Build succeeded`.

- [ ] **Step 5: Commit**

```bash
git add companion/AudioKnubbel.Companion/TrayAppContext.cs
git commit -m "feat(companion): standby submenu with board query + checkmarks"
```

---

## Task 7: Gesamtverifikation (ohne Gerät)

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

Run: `git diff --stat $(git merge-base HEAD master)..HEAD`
Expected: nur die geplanten Dateien geändert.

---

## Deferred — Hardware-Verifikation (nach „go" + freiem Gerät)

1. Firmware flashen (Board im Flash-Mode, COM5).
2. Tray → Standby → Stufe wählen → Display geht nach n s aus.
3. Board-Reboot → Wert bleibt (NVS).
4. Connect → Haken steht auf echtem Board-Wert (STBY?-Query).
5. Optional Serial: `STBY:30` / `STBY?` → `STBY:30`.

Nach erfolgreichem E2E: Handoff aktualisieren.

---

## Self-Review-Notiz

- **Spec-Abdeckung:** Wertebereich/Schritte + Bold 5/15/30/60 (Task 6), Default 15 (Task 3),
  Protokoll `STBY:`/`STBY?` (Tasks 1,2,4), NVS (Task 3), Getter-getriebener Check (Task 4),
  App-Query/Haken (Tasks 5,6), Tests (Tasks 1,7). Alle Punkte abgedeckt.
- **Typ-Konsistenz:** `standbyCfgInit/Set/Get`, `protocolApplyStandby`,
  `Protocol.StandbyLine`/`QueryStandbyLine`/`TryParseStandby`, `SerialLink.QueryStandby`,
  `_standbyMenu`/`_standby`/`SetStandby`/`UpdateStandbyChecks`/`QueryStandbyAsync` — über alle
  Tasks identisch.
- **Kein Hardware-Seiteneffekt** beim Setzen (anders als Backlight) — korrekt, nur Schwelle.
