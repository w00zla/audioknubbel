# Milestone 3 — C#-Companion + echter Volume-State — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Der Lautstärke-Arc des audioknubbel zeigt den echten Windows-Master-Volume + Mute, geliefert von einer C#-Tray-App über USB-CDC-Serial; ohne App bleibt das heutige HID-/Schätzwert-Verhalten erhalten.

**Architecture:** Hybrid Closed Loop — Encoder bleibt HID-Aktuator, die C#-App liest den echten Zustand via NAudio und pusht `VOL:`/`MUTE:`-Zeilen aufs Board, das seinen lokalen Schätzwert damit überschreibt. Firmware bekommt einen kleinen zeilenbasierten RX-Parser; die App ist event-getrieben mit Auto-Reconnect und VID/PID-Port-Discovery.

**Tech Stack:** ESP32-S3 / Arduino-Core 3.x (pioarduino 53.03.13), LVGL 8.3, Unity (native tests) · .NET 8 WinForms (`net8.0-windows`), NAudio 2.2, System.Management (WMI), xUnit.

**Referenz-Spec:** `docs/superpowers/specs/2026-06-11-milestone3-companion-design.md`

---

## File Structure

**Firmware (ESP32-S3):**
- `src/protocol.h` *(neu)* — Pure Parser `protocolParseLine()` (native-testbar) + `protocolPoll()`-Deklaration (`#ifdef ARDUINO`).
- `src/protocol.cpp` *(neu)* — `protocolPoll()`: liest `USBSerial`, akkumuliert Zeilen, ruft Apply-Hooks / ID-Reply.
- `src/main.cpp` *(mod)* — `protocolApplyVolume()/protocolApplyMute()` Hooks; `protocolPoll()` im Loop.
- `src/hid.cpp` *(mod)* — `USB.productName("audioknubbel")` vor `USB.begin()`.
- `platformio.ini` *(mod)* — neues `[env:native]` für Host-Tests.
- `test/native/test_protocol_parser/test_main.cpp` *(neu)* — Unity-Tests für den Parser.

**C#-Companion (`companion/`):**
- `AudioKnubbel.Companion.sln` *(neu)*
- `AudioKnubbel.Companion/AudioKnubbel.Companion.csproj` *(neu)* — WinExe, net8.0-windows, NAudio, System.Management.
- `AudioKnubbel.Companion/Protocol.cs` *(neu)* — formatiert `VOL:`/`MUTE:`-Zeilen (pure, testbar).
- `AudioKnubbel.Companion/Contracts.cs` *(neu)* — `AudioState`, `IVolumeSource`, `ISerialSink`.
- `AudioKnubbel.Companion/SyncController.cs` *(neu)* — Dedup + Full-State-Logik (pure, testbar).
- `AudioKnubbel.Companion/PortDiscovery.cs` *(neu)* — COM-Port via VID/PID (WMI).
- `AudioKnubbel.Companion/SerialLink.cs` *(neu)* — SerialPort + Auto-Reconnect (`ISerialSink`).
- `AudioKnubbel.Companion/VolumeMonitor.cs` *(neu)* — NAudio Volume/Mute + Device-Wechsel (`IVolumeSource`).
- `AudioKnubbel.Companion/TrayAppContext.cs` *(neu)* — NotifyIcon + 40 ms-Coalescing + Reconnect-Timer.
- `AudioKnubbel.Companion/Program.cs` *(neu)* — Entry + Single-Instance-Mutex.
- `AudioKnubbel.Companion.Tests/AudioKnubbel.Companion.Tests.csproj` *(neu)* — xUnit.
- `AudioKnubbel.Companion.Tests/FakeSink.cs` *(neu)* — Test-Doubles.
- `AudioKnubbel.Companion.Tests/ProtocolTests.cs` *(neu)*
- `AudioKnubbel.Companion.Tests/SyncControllerTests.cs` *(neu)*

**Konventionen:**
- `pio.exe` Vollpfad: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe`. Flash-Env: `crowpanel-s3`, Port COM5.
- **Flash-Regel:** Claude buildet, meldet „ready to flash", **wartet auf „go" vom User** (Device im Download-Modus), erst dann `-t upload`. Native-Tests und C#-Builds brauchen kein „go".
- Hardware-Tabu: `display.cpp`, GPIO1/2/40/46 **nicht** anfassen.

---

## Task 1: Firmware — Serial-Parser (pure, native-getestet)

**Files:**
- Modify: `platformio.ini`
- Create: `src/protocol.h`
- Test: `test/native/test_protocol_parser/test_main.cpp`

- [ ] **Step 1: `[env:native]` in `platformio.ini` ergänzen**

Am Dateiende anhängen:

```ini
[env:native]
platform = native
test_framework = unity
build_flags = -I src
```

- [ ] **Step 2: Failing test schreiben** — `test/native/test_protocol_parser/test_main.cpp`

```cpp
#include <unity.h>
#include "../../../src/protocol.h"

void setUp() {}
void tearDown() {}

void test_vol_basic() {
    ProtoResult r = protocolParseLine("VOL:42");
    TEST_ASSERT_EQUAL(int(ProtoCmd::SetVolume), int(r.cmd));
    TEST_ASSERT_EQUAL(42, r.value);
}
void test_vol_clamp_high() {
    TEST_ASSERT_EQUAL(100, protocolParseLine("VOL:250").value);
}
void test_vol_clamp_low_and_cr() {
    ProtoResult r = protocolParseLine("VOL:-5\r");
    TEST_ASSERT_EQUAL(int(ProtoCmd::SetVolume), int(r.cmd));
    TEST_ASSERT_EQUAL(0, r.value);
}
void test_mute_on() {
    ProtoResult r = protocolParseLine("MUTE:1");
    TEST_ASSERT_EQUAL(int(ProtoCmd::SetMute), int(r.cmd));
    TEST_ASSERT_EQUAL(1, r.value);
}
void test_mute_off() {
    TEST_ASSERT_EQUAL(0, protocolParseLine("MUTE:0").value);
}
void test_identify() {
    TEST_ASSERT_EQUAL(int(ProtoCmd::Identify), int(protocolParseLine("ID?").cmd));
}
void test_unknown_and_empty() {
    TEST_ASSERT_EQUAL(int(ProtoCmd::None), int(protocolParseLine("GARBAGE").cmd));
    TEST_ASSERT_EQUAL(int(ProtoCmd::None), int(protocolParseLine("").cmd));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_vol_basic);
    RUN_TEST(test_vol_clamp_high);
    RUN_TEST(test_vol_clamp_low_and_cr);
    RUN_TEST(test_mute_on);
    RUN_TEST(test_mute_off);
    RUN_TEST(test_identify);
    RUN_TEST(test_unknown_and_empty);
    return UNITY_END();
}
```

- [ ] **Step 3: Test laufen lassen, FAIL bestätigen**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe test -e native`
Expected: Compile-Fehler (`protocol.h` / `protocolParseLine` existiert nicht).

- [ ] **Step 4: `src/protocol.h` schreiben (pure Parser)**

```cpp
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum class ProtoCmd : uint8_t { None, SetVolume, SetMute, Identify };

struct ProtoResult {
    ProtoCmd cmd;
    int      value;   // SetVolume: 0..100 (geclampt); SetMute: 0/1
};

// Pure line parser — kein Arduino, native-testbar (wie encoderQuadStep).
// Toleriert führende Whitespaces und trailing \r. Unbekannt/leer -> None.
inline ProtoResult protocolParseLine(const char* line) {
    ProtoResult r{ProtoCmd::None, 0};
    if (!line) return r;
    while (*line == ' ' || *line == '\t') line++;

    if (strncmp(line, "VOL:", 4) == 0) {
        int v = atoi(line + 4);
        if (v < 0)   v = 0;
        if (v > 100) v = 100;
        r.cmd = ProtoCmd::SetVolume;
        r.value = v;
    } else if (strncmp(line, "MUTE:", 5) == 0) {
        r.cmd = ProtoCmd::SetMute;
        r.value = (atoi(line + 5) != 0) ? 1 : 0;
    } else if (strncmp(line, "ID?", 3) == 0) {
        r.cmd = ProtoCmd::Identify;
    }
    return r;
}

#ifdef ARDUINO
void protocolPoll();   // liest USBSerial, wendet Kommandos an (siehe protocol.cpp)
#endif
```

- [ ] **Step 5: Test laufen lassen, PASS bestätigen**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe test -e native`
Expected: Alle Tests PASS (auch der bestehende `test_encoder_logic`).

- [ ] **Step 6: Commit**

```bash
git add platformio.ini src/protocol.h test/native/test_protocol_parser/
git commit -m "feat(fw): serial protocol line parser + native test env

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Firmware — protocolPoll + Integration

**Files:**
- Create: `src/protocol.cpp`
- Modify: `src/main.cpp`, `src/hid.cpp`

- [ ] **Step 1: `src/protocol.cpp` schreiben**

```cpp
#include "protocol.h"
#ifdef ARDUINO
#include <Arduino.h>
#include "hid.h"   // USBSerial
#include "ui.h"

// In main.cpp definiert — hält lokalen Schätzwert + Realwert konsistent.
extern void protocolApplyVolume(int pct);
extern void protocolApplyMute(bool muted);

static char    s_buf[64];
static uint8_t s_len = 0;

static void handleLine(const char* line) {
    ProtoResult r = protocolParseLine(line);
    switch (r.cmd) {
        case ProtoCmd::SetVolume: protocolApplyVolume(r.value);        break;
        case ProtoCmd::SetMute:   protocolApplyMute(r.value != 0);     break;
        case ProtoCmd::Identify:  USBSerial.println("AUDIOKNUBBEL M3");    break;
        case ProtoCmd::None:                                           break;
    }
}

void protocolPoll() {
    while (USBSerial.available() > 0) {
        char c = (char)USBSerial.read();
        if (c == '\n') {
            s_buf[s_len] = '\0';
            handleLine(s_buf);
            s_len = 0;
        } else if (c != '\r') {
            if (s_len < sizeof(s_buf) - 1) {
                s_buf[s_len++] = c;
            } else {
                s_len = 0;   // Overflow: Zeile verwerfen
            }
        }
    }
}
#endif
```

- [ ] **Step 2: `src/main.cpp` anpassen — Apply-Hooks + Poll im Loop**

Ersetze den kompletten Inhalt von `src/main.cpp` durch:

```cpp
#include <Arduino.h>
#include <lvgl.h>
#include "encoder.h"
#include "hid.h"
#include "display.h"
#include "ui.h"
#include "protocol.h"

// Lokaler Lautstärke-Schätzwert (Fallback, wenn die Companion-App nicht läuft).
// Die App überschreibt ihn via protocolApply* mit dem echten Windows-Wert.
static int  s_volume_pct = 50;
static bool s_muted      = false;

// Vom Serial-Parser aufgerufen, wenn die App den echten State pusht.
void protocolApplyVolume(int pct) {
    s_volume_pct = constrain(pct, 0, 100);
    ui_set_volume(s_volume_pct);
}
void protocolApplyMute(bool muted) {
    s_muted = muted;
    ui_set_mute(s_muted);
}

void setup() {
    hidInit();                       // USB HID (Consumer Control) + CDC + Product-Name
    USBSerial.setTxTimeoutMs(0);     // nicht blockieren ohne offenen Serial-Monitor
    encoderInit();
    displayInit();                   // Power-Rails, GC9A01, Backlight, LVGL
    ui_init();                       // Arc + Mute-Label
    USBSerial.println("[BOOT] audioknubbel M3 ready");
}

void loop() {
    int ticks = encoderGetTicks();
    if (ticks != 0) {
        s_volume_pct = constrain(s_volume_pct - ticks * 2, 0, 100);
        ui_set_volume(s_volume_pct);   // optimistisches Sofort-Feedback
        hidVolumeStep(-ticks);         // HID ändert echten Windows-Volume
    }
    if (encoderGetPress()) {
        s_muted = !s_muted;
        ui_set_mute(s_muted);
        hidMuteToggle();
    }
    protocolPoll();                    // App pusht echten State -> protocolApply*
    lv_timer_handler();                // LVGL rendern
    delay(5);
}
```

- [ ] **Step 3: `src/hid.cpp` — Product-Name setzen**

Ersetze die `hidInit()`-Funktion durch:

```cpp
void hidInit() {
    sConsumer.begin();
    USBSerial.begin();
    USB.productName("audioknubbel");        // Discovery-Tiebreaker für die Companion-App
    USB.manufacturerName("audioknubbel");
    USB.begin();
}
```

Und ergänze oben den Include (falls noch nicht vorhanden): `USB.h` ist bereits inkludiert — kein neuer Include nötig.

- [ ] **Step 4: Build (kein Flash)**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS` — kompiliert ohne Fehler.

- [ ] **Step 5: Native-Tests erneut grün**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe test -e native`
Expected: Alle PASS (protocol.cpp wird im native-Build via `#ifdef ARDUINO` ausgeklammert).

- [ ] **Step 6: Flash — NACH „go" vom User**

Melde „ready to flash". Warte auf „go" (User setzt Device in Download-Modus). Dann:
Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3 -t upload`
Expected: Upload erfolgreich, Board rebootet, Serial zeigt `[BOOT] audioknubbel M3 ready`.

- [ ] **Step 7: Manueller Serial-Test**

Sende über einen Serial-Terminal (115200, COM5) zeilenweise:
`VOL:20` → Arc springt auf 20 %. `VOL:90` → Arc auf 90 %. `MUTE:1` → grau + „MUTE". `MUTE:0` → wieder cyan. `ID?` → Board antwortet `AUDIOKNUBBEL M3`.
Encoder drehen → Arc bewegt sich weiterhin (HID-Fallback unberührt).

- [ ] **Step 8: Commit**

```bash
git add src/protocol.cpp src/main.cpp src/hid.cpp
git commit -m "feat(fw): apply pushed volume/mute state + USB product name

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: C#-App — Solution + Projekt-Scaffold (grüner Build)

**Files:**
- Create: `companion/AudioKnubbel.Companion.sln`, `companion/AudioKnubbel.Companion/AudioKnubbel.Companion.csproj`, `companion/AudioKnubbel.Companion.Tests/AudioKnubbel.Companion.Tests.csproj`

- [ ] **Step 1: .NET 8 SDK prüfen**

Run: `dotnet --version`
Expected: `8.x` (oder höher). Falls nicht vorhanden → installieren, bevor es weitergeht.

- [ ] **Step 2: `companion/AudioKnubbel.Companion/AudioKnubbel.Companion.csproj` anlegen**

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <UseWindowsForms>true</UseWindowsForms>
    <RootNamespace>AudioKnubbel.Companion</RootNamespace>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="NAudio" Version="2.2.1" />
    <PackageReference Include="System.Management" Version="8.0.0" />
  </ItemGroup>
</Project>
```

- [ ] **Step 3: `companion/AudioKnubbel.Companion.Tests/AudioKnubbel.Companion.Tests.csproj` anlegen**

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0-windows</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <IsPackable>false</IsPackable>
  </PropertyGroup>
  <ItemGroup>
    <PackageReference Include="Microsoft.NET.Test.Sdk" Version="17.11.1" />
    <PackageReference Include="xunit" Version="2.9.2" />
    <PackageReference Include="xunit.runner.visualstudio" Version="2.8.2" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\AudioKnubbel.Companion\AudioKnubbel.Companion.csproj" />
  </ItemGroup>
</Project>
```

- [ ] **Step 4: Solution erzeugen und Projekte hinzufügen**

```bash
cd companion
dotnet new sln -n AudioKnubbel.Companion
dotnet sln add AudioKnubbel.Companion/AudioKnubbel.Companion.csproj
dotnet sln add AudioKnubbel.Companion.Tests/AudioKnubbel.Companion.Tests.csproj
```

- [ ] **Step 5: Temporären Platzhalter, damit das Main-Projekt baut** — `companion/AudioKnubbel.Companion/Program.cs`

```csharp
namespace AudioKnubbel.Companion;

internal static class Program
{
    [System.STAThread]
    private static void Main() { }
}
```

- [ ] **Step 6: Build verifizieren**

Run: `dotnet build companion/AudioKnubbel.Companion.sln`
Expected: Build `succeeded`, NuGet-Pakete (NAudio, System.Management, xUnit) werden restored.

- [ ] **Step 7: Commit**

```bash
git add companion/
git commit -m "chore(app): scaffold .NET 8 WinForms companion solution + test project

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: C#-App — Protocol-Formatierung (TDD)

**Files:**
- Create: `companion/AudioKnubbel.Companion/Protocol.cs`
- Test: `companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs`

- [ ] **Step 1: Failing test** — `companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs`

```csharp
using AudioKnubbel.Companion;
using Xunit;

public class ProtocolTests
{
    [Theory]
    [InlineData(42, "VOL:42\n")]
    [InlineData(0, "VOL:0\n")]
    [InlineData(100, "VOL:100\n")]
    [InlineData(150, "VOL:100\n")]
    [InlineData(-5, "VOL:0\n")]
    public void VolumeLine_ClampsAndFormats(int input, string expected)
        => Assert.Equal(expected, Protocol.VolumeLine(input));

    [Theory]
    [InlineData(true, "MUTE:1\n")]
    [InlineData(false, "MUTE:0\n")]
    public void MuteLine_Formats(bool muted, string expected)
        => Assert.Equal(expected, Protocol.MuteLine(muted));
}
```

- [ ] **Step 2: Test laufen lassen, FAIL bestätigen**

Run: `dotnet test companion/AudioKnubbel.Companion.sln`
Expected: Compile-Fehler — `Protocol` existiert nicht.

- [ ] **Step 3: `companion/AudioKnubbel.Companion/Protocol.cs` schreiben**

```csharp
namespace AudioKnubbel.Companion;

public static class Protocol
{
    public static string VolumeLine(int volume) => $"VOL:{Math.Clamp(volume, 0, 100)}\n";
    public static string MuteLine(bool muted) => muted ? "MUTE:1\n" : "MUTE:0\n";
}
```

- [ ] **Step 4: Test laufen lassen, PASS bestätigen**

Run: `dotnet test companion/AudioKnubbel.Companion.sln`
Expected: 7 Tests PASS.

- [ ] **Step 5: Commit**

```bash
git add companion/AudioKnubbel.Companion/Protocol.cs companion/AudioKnubbel.Companion.Tests/ProtocolTests.cs
git commit -m "feat(app): VOL/MUTE line formatting with clamp

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: C#-App — Contracts + SyncController (TDD)

**Files:**
- Create: `companion/AudioKnubbel.Companion/Contracts.cs`, `companion/AudioKnubbel.Companion/SyncController.cs`
- Test: `companion/AudioKnubbel.Companion.Tests/FakeSink.cs`, `companion/AudioKnubbel.Companion.Tests/SyncControllerTests.cs`

- [ ] **Step 1: Contracts (Interfaces + State) anlegen** — `companion/AudioKnubbel.Companion/Contracts.cs`

```csharp
namespace AudioKnubbel.Companion;

public readonly record struct AudioState(int Volume, bool Muted);

public interface IVolumeSource
{
    AudioState Current { get; }
    event Action<AudioState>? StateChanged;
}

public interface ISerialSink
{
    bool Connected { get; }
    void Send(string line);
}
```

- [ ] **Step 2: FakeSink für Tests** — `companion/AudioKnubbel.Companion.Tests/FakeSink.cs`

```csharp
using AudioKnubbel.Companion;

public sealed class FakeSink : ISerialSink
{
    public bool Connected { get; set; } = true;
    public List<string> Sent { get; } = new();
    public void Send(string line) => Sent.Add(line);
}
```

- [ ] **Step 3: Failing tests** — `companion/AudioKnubbel.Companion.Tests/SyncControllerTests.cs`

```csharp
using AudioKnubbel.Companion;
using Xunit;

public class SyncControllerTests
{
    [Fact]
    public void FirstSync_SendsFullState()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        Assert.Equal(new[] { "VOL:42\n", "MUTE:0\n" }, sink.Sent);
    }

    [Fact]
    public void RepeatSameState_SendsNothing()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Sync(new AudioState(42, false));
        Assert.Empty(sink.Sent);
    }

    [Fact]
    public void VolumeChange_SendsOnlyVolume()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Sync(new AudioState(43, false));
        Assert.Equal(new[] { "VOL:43\n" }, sink.Sent);
    }

    [Fact]
    public void MuteChange_SendsOnlyMute()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Sync(new AudioState(42, true));
        Assert.Equal(new[] { "MUTE:1\n" }, sink.Sent);
    }

    [Fact]
    public void Disconnected_SendsNothing()
    {
        var sink = new FakeSink { Connected = false };
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        Assert.Empty(sink.Sent);
    }

    [Fact]
    public void AfterReset_SendsFullStateAgain()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Reset();
        c.Sync(new AudioState(42, false));
        Assert.Equal(new[] { "VOL:42\n", "MUTE:0\n" }, sink.Sent);
    }
}
```

- [ ] **Step 4: Test laufen lassen, FAIL bestätigen**

Run: `dotnet test companion/AudioKnubbel.Companion.sln`
Expected: Compile-Fehler — `SyncController` existiert nicht.

- [ ] **Step 5: `companion/AudioKnubbel.Companion/SyncController.cs` schreiben**

```csharp
namespace AudioKnubbel.Companion;

// Kern-Logik (testbar): spiegelt Audio-State auf den Serial-Sink.
// - Nichts senden, wenn nicht verbunden
// - Nur Senden bei tatsächlicher Änderung (Dedup pro Feld)
// - Reset() erzwingt beim nächsten Sync den vollen State (für Reconnect)
public sealed class SyncController
{
    private readonly ISerialSink _sink;
    private AudioState? _lastSent;

    public SyncController(ISerialSink sink) => _sink = sink;

    public void Reset() => _lastSent = null;

    public void Sync(AudioState s)
    {
        if (!_sink.Connected) return;

        if (_lastSent is { } last)
        {
            if (s.Volume != last.Volume) _sink.Send(Protocol.VolumeLine(s.Volume));
            if (s.Muted != last.Muted)   _sink.Send(Protocol.MuteLine(s.Muted));
        }
        else
        {
            _sink.Send(Protocol.VolumeLine(s.Volume));
            _sink.Send(Protocol.MuteLine(s.Muted));
        }
        _lastSent = s;
    }
}
```

- [ ] **Step 6: Test laufen lassen, PASS bestätigen**

Run: `dotnet test companion/AudioKnubbel.Companion.sln`
Expected: Alle Tests PASS (6 neue + 7 Protocol).

- [ ] **Step 7: Commit**

```bash
git add companion/AudioKnubbel.Companion/Contracts.cs companion/AudioKnubbel.Companion/SyncController.cs companion/AudioKnubbel.Companion.Tests/FakeSink.cs companion/AudioKnubbel.Companion.Tests/SyncControllerTests.cs
git commit -m "feat(app): SyncController dedup + full-state logic with contracts

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: C#-App — PortDiscovery + SerialLink

**Files:**
- Create: `companion/AudioKnubbel.Companion/PortDiscovery.cs`, `companion/AudioKnubbel.Companion/SerialLink.cs`

*Hinweis: WMI + echte COM-Ports sind OS-abhängig → nicht unit-getestet, manuell verifiziert.*

- [ ] **Step 1: `companion/AudioKnubbel.Companion/PortDiscovery.cs` schreiben**

```csharp
using System.Management;

namespace AudioKnubbel.Companion;

// Findet den COM-Port des audioknubbel über die Espressif-VID (0x303A).
public static class PortDiscovery
{
    private const string Vid = "VID_303A";

    public static string? FindPort()
    {
        using var searcher = new ManagementObjectSearcher(
            "SELECT Name, PNPDeviceID FROM Win32_PnPEntity WHERE Name LIKE '%(COM%)'");
        foreach (ManagementBaseObject device in searcher.Get())
        {
            var pnpId = device["PNPDeviceID"]?.ToString() ?? "";
            if (!pnpId.Contains(Vid, StringComparison.OrdinalIgnoreCase)) continue;

            var name = device["Name"]?.ToString() ?? "";
            int open = name.LastIndexOf("(COM", StringComparison.OrdinalIgnoreCase);
            int close = name.LastIndexOf(')');
            if (open >= 0 && close > open)
                return name.Substring(open + 1, close - open - 1); // z.B. "COM5"
        }
        return null;
    }
}
```

- [ ] **Step 2: `companion/AudioKnubbel.Companion/SerialLink.cs` schreiben**

```csharp
using System.IO.Ports;

namespace AudioKnubbel.Companion;

// SerialPort-Wrapper mit VID/PID-Discovery + Auto-Reconnect.
public sealed class SerialLink : ISerialSink, IDisposable
{
    private readonly object _gate = new();
    private SerialPort? _port;

    public event Action<bool>? ConnectionChanged;

    public bool Connected
    {
        get { lock (_gate) return _port?.IsOpen == true; }
    }

    // Versucht zu (re)connecten; true bei Erfolg. Feuert ConnectionChanged(true).
    public bool TryConnect()
    {
        lock (_gate)
        {
            if (_port?.IsOpen == true) return true;

            var portName = PortDiscovery.FindPort();
            if (portName is null) return false;
            try
            {
                var p = new SerialPort(portName, 115200)
                {
                    NewLine = "\n",
                    WriteTimeout = 500,
                    ReadTimeout = 500,
                };
                p.Open();
                _port = p;
            }
            catch
            {
                _port = null;
                return false;
            }
        }
        ConnectionChanged?.Invoke(true);
        return true;
    }

    public void Send(string line)
    {
        bool dropped = false;
        lock (_gate)
        {
            if (_port?.IsOpen != true) return;
            try { _port.Write(line); }
            catch
            {
                try { _port.Close(); } catch { }
                _port.Dispose();
                _port = null;
                dropped = true;
            }
        }
        if (dropped) ConnectionChanged?.Invoke(false);
    }

    public void Dispose()
    {
        lock (_gate)
        {
            try { _port?.Close(); } catch { }
            _port?.Dispose();
            _port = null;
        }
    }
}
```

- [ ] **Step 3: Build verifizieren**

Run: `dotnet build companion/AudioKnubbel.Companion.sln`
Expected: Build `succeeded` (Tests weiterhin grün; keine neuen Tests).

- [ ] **Step 4: Commit**

```bash
git add companion/AudioKnubbel.Companion/PortDiscovery.cs companion/AudioKnubbel.Companion/SerialLink.cs
git commit -m "feat(app): VID/PID port discovery + SerialLink with reconnect

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: C#-App — VolumeMonitor (NAudio + Device-Wechsel)

**Files:**
- Create: `companion/AudioKnubbel.Companion/VolumeMonitor.cs`

*Hinweis: NAudio/CoreAudio ist OS-abhängig → manuell verifiziert.*

- [ ] **Step 1: `companion/AudioKnubbel.Companion/VolumeMonitor.cs` schreiben**

```csharp
using NAudio.CoreAudioApi;
using NAudio.CoreAudioApi.Interfaces;

namespace AudioKnubbel.Companion;

// Liest Master-Volume + Mute des Default-Render-Geräts und meldet Änderungen.
// Folgt automatisch dem Default-Device (Kopfhörer rein/raus) via IMMNotificationClient.
public sealed class VolumeMonitor : IVolumeSource, IMMNotificationClient, IDisposable
{
    private readonly MMDeviceEnumerator _enumerator = new();
    private readonly object _gate = new();
    private MMDevice? _device;

    public event Action<AudioState>? StateChanged;

    public VolumeMonitor()
    {
        _enumerator.RegisterEndpointNotificationCallback(this);
        Attach();
    }

    public AudioState Current
    {
        get
        {
            lock (_gate)
            {
                if (_device is null) return new AudioState(0, false);
                var vol = (int)Math.Round(
                    _device.AudioEndpointVolume.MasterVolumeLevelScalar * 100);
                return new AudioState(vol, _device.AudioEndpointVolume.Mute);
            }
        }
    }

    private void Attach()
    {
        lock (_gate)
        {
            Detach();
            try
            {
                _device = _enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
                _device.AudioEndpointVolume.OnVolumeNotification += OnVolumeNotification;
            }
            catch { _device = null; }
        }
        StateChanged?.Invoke(Current);
    }

    private void Detach()
    {
        if (_device is null) return;
        try { _device.AudioEndpointVolume.OnVolumeNotification -= OnVolumeNotification; } catch { }
        try { _device.Dispose(); } catch { }
        _device = null;
    }

    private void OnVolumeNotification(AudioVolumeNotificationData data)
        => StateChanged?.Invoke(Current);

    // --- IMMNotificationClient ---
    public void OnDefaultDeviceChanged(DataFlow flow, Role role, string defaultDeviceId)
    {
        if (flow == DataFlow.Render && role == Role.Multimedia) Attach();
    }
    public void OnDeviceStateChanged(string deviceId, DeviceState newState) { }
    public void OnDeviceAdded(string pwstrDeviceId) { }
    public void OnDeviceRemoved(string deviceId) { }
    public void OnPropertyValueChanged(string pwstrDeviceId, PropertyKey key) { }

    public void Dispose()
    {
        try { _enumerator.UnregisterEndpointNotificationCallback(this); } catch { }
        lock (_gate) Detach();
        _enumerator.Dispose();
    }
}
```

- [ ] **Step 2: Build verifizieren**

Run: `dotnet build companion/AudioKnubbel.Companion.sln`
Expected: Build `succeeded`. (Falls API-Namen abweichen: NAudio 2.2 nutzt `MasterVolumeLevelScalar`, `Mute`, `OnVolumeNotification`, `RegisterEndpointNotificationCallback` — bei Fehler Signaturen gegen die installierte NAudio-Version prüfen.)

- [ ] **Step 3: Commit**

```bash
git add companion/AudioKnubbel.Companion/VolumeMonitor.cs
git commit -m "feat(app): NAudio volume monitor with default-device follow

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: C#-App — TrayAppContext + Program (Verdrahtung + E2E)

**Files:**
- Modify: `companion/AudioKnubbel.Companion/Program.cs`
- Create: `companion/AudioKnubbel.Companion/TrayAppContext.cs`

- [ ] **Step 1: `companion/AudioKnubbel.Companion/TrayAppContext.cs` schreiben**

```csharp
using System.Drawing;
using System.Windows.Forms;

namespace AudioKnubbel.Companion;

// Verdrahtet VolumeMonitor -> 40ms-Coalescing -> SyncController -> SerialLink,
// hält ein Tray-Icon und einen Reconnect-Timer.
public sealed class TrayAppContext : ApplicationContext
{
    private readonly VolumeMonitor _monitor = new();
    private readonly SerialLink _link = new();
    private readonly SyncController _sync;
    private readonly NotifyIcon _tray;
    private readonly System.Windows.Forms.Timer _reconnect;
    private readonly System.Windows.Forms.Timer _debounce;
    private readonly SynchronizationContext _ui;

    private AudioState _pending;
    private bool _hasPending;

    public TrayAppContext()
    {
        _ui = SynchronizationContext.Current!;
        _sync = new SyncController(_link);

        _tray = new NotifyIcon
        {
            Icon = SystemIcons.Application,
            Text = "audioknubbel: suche Board…",
            Visible = true,
            ContextMenuStrip = BuildMenu(),
        };

        _link.ConnectionChanged += OnConnectionChanged;
        _monitor.StateChanged += OnVolumeStateChanged;

        _debounce = new System.Windows.Forms.Timer { Interval = 40 };
        _debounce.Tick += (_, _) =>
        {
            _debounce.Stop();
            if (_hasPending) { _hasPending = false; _sync.Sync(_pending); }
        };

        _reconnect = new System.Windows.Forms.Timer { Interval = 2000 };
        _reconnect.Tick += (_, _) => { if (!_link.Connected) _link.TryConnect(); };
        _reconnect.Start();

        _link.TryConnect();   // sofortiger erster Versuch
    }

    private ContextMenuStrip BuildMenu()
    {
        var menu = new ContextMenuStrip();
        menu.Items.Add("Reconnect", null, (_, _) => _link.TryConnect());
        menu.Items.Add("Exit", null, (_, _) => ExitThread());
        return menu;
    }

    // Feuert ggf. auf NAudio-COM-Thread -> auf UI-Thread marshallen + coalescen.
    private void OnVolumeStateChanged(AudioState s)
        => _ui.Post(_ => { _pending = s; _hasPending = true; _debounce.Stop(); _debounce.Start(); }, null);

    // Feuert aus SerialLink -> auf UI-Thread marshallen.
    private void OnConnectionChanged(bool connected) => _ui.Post(_ =>
    {
        _tray.Text = connected ? "audioknubbel: verbunden" : "audioknubbel: suche Board…";
        if (connected)
        {
            _sync.Reset();
            _sync.Sync(_monitor.Current);   // voller State direkt nach Connect
        }
    }, null);

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _reconnect.Dispose();
            _debounce.Dispose();
            _tray.Visible = false;
            _tray.Dispose();
            _monitor.Dispose();
            _link.Dispose();
        }
        base.Dispose(disposing);
    }
}
```

- [ ] **Step 2: `companion/AudioKnubbel.Companion/Program.cs` ersetzen**

```csharp
using System.Windows.Forms;

namespace AudioKnubbel.Companion;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        using var mutex = new Mutex(true, "AudioKnubbel.Companion.SingleInstance", out bool isNew);
        if (!isNew) return;   // bereits eine Instanz aktiv

        ApplicationConfiguration.Initialize();
        Application.Run(new TrayAppContext());
    }
}
```

- [ ] **Step 3: Build + Tests grün**

Run: `dotnet build companion/AudioKnubbel.Companion.sln` und `dotnet test companion/AudioKnubbel.Companion.sln`
Expected: Build `succeeded`, alle Unit-Tests PASS.

- [ ] **Step 4: E2E-Test (Board geflasht aus Task 2, App starten)**

Run: `dotnet run --project companion/AudioKnubbel.Companion`
Verifizieren:
1. Tray-Icon erscheint, Tooltip wechselt auf „verbunden", Arc springt auf den echten Windows-Volume.
2. Windows-Lautstärke-Slider (Taskleiste) ziehen → Arc folgt.
3. Encoder am Board drehen → Windows-Volume ändert sich (HID), Arc snappt auf den exakten Wert.
4. Encoder drücken → Mute, Arc grau + „MUTE"; nochmal → entmutet.
5. Default-Audio-Gerät wechseln (z.B. Kopfhörer) → Arc zeigt dessen Volume.
6. App per Tray „Exit" beenden → Encoder funktioniert weiter (HID-Fallback, Schätzwert).
7. USB neu stecken / App neu starten → reconnectet automatisch innerhalb ~2 s.

- [ ] **Step 5: Commit**

```bash
git add companion/AudioKnubbel.Companion/TrayAppContext.cs companion/AudioKnubbel.Companion/Program.cs
git commit -m "feat(app): tray context wiring, debounce, reconnect, single-instance

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review (Plan vs. Spec)

**Spec coverage:**
- Master-Volume + Mute lesen/pushen → Task 7 (VolumeMonitor) + Task 5 (SyncController) + Task 2 (Firmware apply). ✔
- Externe Änderungen reflektieren → `OnVolumeNotification` in Task 7. ✔
- Output-Device-Wechsel → `IMMNotificationClient` in Task 7. ✔
- Hybrid / HID bleibt Aktuator → Task 2 Loop unverändert (HID), App nur Leser. ✔
- Graceful Degradation → Task 2 lokaler Schätzwert bleibt; Task 8 Step 4.6 testet App-Exit. ✔
- VID/PID-Discovery + Product-String → Task 6 (PortDiscovery) + Task 2 (`USB.productName`). ✔
- Event-getrieben + Debounce + Full-State-on-Connect → Task 8 (Coalescing + OnConnectionChanged Reset/Sync). ✔
- Protokoll `VOL:`/`MUTE:`/`ID?`/`AUDIOKNUBBEL` → Task 1 (Parse) + Task 2 (Poll/Reply) + Task 4 (Format). ✔
- Robustheit (unknown/empty/CR/overflow/clamp) → Task 1 Tests + Task 2 Overflow. ✔
- Testing-Strategie → native Unity (Task 1), xUnit (Task 4/5), manuelle E2E (Task 2/8). ✔
- Per-App-Volume → bewusst Out of Scope, kein Task. ✔

**Placeholder scan:** Keine TBD/TODO; jeder Code-Step enthält vollständigen Code, jeder Run-Step ein erwartetes Ergebnis.

**Type consistency:** `AudioState(int Volume, bool Muted)`, `ISerialSink.{Connected,Send}`, `IVolumeSource.{Current,StateChanged}`, `SyncController.{Sync,Reset}`, `ProtoCmd`/`ProtoResult`, `protocolApplyVolume/Mute`, `protocolPoll` durchgängig identisch in Definition und Verwendung. ✔
