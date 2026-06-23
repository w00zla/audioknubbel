# audioknubbel Firmware Milestone 1 Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PlatformIO-Projekt für den CrowPanel ESP32-S3 aufsetzen — Encoder dreht → Windows-Lautstärke, Encoder drückt → Mute-Toggle, Serial-Debug läuft parallel über USB-CDC.

**Architecture:** Drei Translation Units: `encoder` kapselt ISR-basiertes Quadratur-Decoding (mit testbarer Pure-Logic-Funktion) und Switch-Debounce; `hid` kapselt den USBHIDConsumerControl-Wrapper inkl. `USB.begin()`; `main` verdrahtet beide. USB-Composite (CDC + HID) via pioarduino-Fork mit ESP32 Arduino Core 3.x. Die reine Decode-Logik ist header-only und wird mit Unity auf dem `native`-Environment getestet.

**Tech Stack:** PlatformIO, pioarduino platform-espressif32 53.03.13, ESP32 Arduino Core 3.x, USBHIDConsumerControl (Core built-in), Unity (PlatformIO native test framework)

---

## File Map

| Pfad | Verantwortung |
|---|---|
| `platformio.ini` | Board-Config, Upload, build_flags, native Test-Env |
| `src/encoder.h` | Public API + `encoderQuadStep()` inline (keine Arduino-Deps → native-testbar) |
| `src/encoder.cpp` | ISR, portMUX, Switch-Debounce, akkumulierte Ticks |
| `src/hid.h` | Public API: `hidInit()`, `hidVolumeStep()`, `hidMuteToggle()` |
| `src/hid.cpp` | USBHIDConsumerControl-Instanz, `USB.begin()` |
| `src/main.cpp` | `setup()` / `loop()` — verdrahtet encoder + hid, Serial-Debug |
| `test/native/test_encoder_logic/test_main.cpp` | Unity-Tests für `encoderQuadStep()` — kein Board nötig |

---

## Chunk 1: Projekt-Scaffold

### Task 1: `platformio.ini` anlegen

**Files:**
- Create: `platformio.ini`

- [ ] **Schritt 1: Datei anlegen**

```ini
[env:crowpanel-s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.13/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
; USB Composite: CDC (Serial) + HID gleichzeitig
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

[env:native]
platform = native
test_framework = unity
```

- [ ] **Schritt 2: PlatformIO-Platform herunterladen und Environments prüfen**

```
pio run --list-targets -e crowpanel-s3
pio run --list-targets -e native
```

Erwartet: beide Environments werden ohne Fehler aufgelistet. Der erste Aufruf lädt die pioarduino-Platform (~200 MB).

---

### Task 2: Leere Quelldateien anlegen

**Files:**
- Create: `src/encoder.h`, `src/encoder.cpp`, `src/hid.h`, `src/hid.cpp`, `src/main.cpp`
- Create: `test/native/test_encoder_logic/test_main.cpp`

- [ ] **Schritt 1: Stub-Dateien anlegen**

`src/encoder.h` — Stub:
```cpp
#pragma once
#include <stdint.h>
```

`src/encoder.cpp` — Stub:
```cpp
#include "encoder.h"
```

`src/hid.h` — Stub:
```cpp
#pragma once
```

`src/hid.cpp` — Stub:
```cpp
#include "hid.h"
```

`src/main.cpp` — Stub:
```cpp
void setup() {}
void loop() {}
```

`test/native/test_encoder_logic/test_main.cpp` — Stub:
```cpp
#include <unity.h>
void setUp() {}
void tearDown() {}
int main() {
    UNITY_BEGIN();
    return UNITY_END();
}
```

- [ ] **Schritt 2: Stub-Build verifizieren**

```
pio run -e crowpanel-s3
```

Erwartet: Kompiliert ohne Fehler (leere Implementierung).

- [ ] **Schritt 3: Commit**

```
git add platformio.ini src/ test/
git commit -m "feat: project scaffold with platformio.ini and stub sources"
```

---

## Chunk 2: Encoder-Logik (TDD, native)

### Task 3: Failing Tests für Quadratur-Dekodierung schreiben

**Files:**
- Modify: `test/native/test_encoder_logic/test_main.cpp`

Die Quadratur-Zustandsmaschine kodiert Drehrichtung als 2-Bit-Zustand (A<<1|B).
CW-Sequenz: `00→01→11→10→00` (+1 pro Schritt).
CCW-Sequenz: `00→10→11→01→00` (-1 pro Schritt).

- [ ] **Schritt 1: Tests schreiben**

```cpp
#include <unity.h>
#include "../../src/encoder.h"   // encoderQuadStep() ist darin inline definiert

void setUp() {}
void tearDown() {}

void test_cw_full_cycle() {
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b00, 0b01));
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b01, 0b11));
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b11, 0b10));
    TEST_ASSERT_EQUAL( 1, encoderQuadStep(0b10, 0b00));
}

void test_ccw_full_cycle() {
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b00, 0b10));
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b10, 0b11));
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b11, 0b01));
    TEST_ASSERT_EQUAL(-1, encoderQuadStep(0b01, 0b00));
}

void test_no_change() {
    TEST_ASSERT_EQUAL(0, encoderQuadStep(0b00, 0b00));
    TEST_ASSERT_EQUAL(0, encoderQuadStep(0b11, 0b11));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cw_full_cycle);
    RUN_TEST(test_ccw_full_cycle);
    RUN_TEST(test_no_change);
    return UNITY_END();
}
```

- [ ] **Schritt 2: Tests ausführen — müssen FEHLSCHLAGEN**

```
pio test -e native
```

Erwartet: FAIL — `encoderQuadStep` ist noch nicht definiert.

---

### Task 4: `encoderQuadStep()` implementieren

**Files:**
- Modify: `src/encoder.h`

- [ ] **Schritt 1: Funktion in encoder.h inline definieren (keine Arduino-Deps)**

```cpp
#pragma once
#include <stdint.h>

// Pure quadrature decode — kein Arduino, daher native-testbar.
// prev_ab: vorheriger Zustand (A<<1|B), curr_ab: aktueller Zustand.
// Gibt +1 (CW), -1 (CCW) oder 0 (keine/ungültige Änderung) zurück.
inline int8_t encoderQuadStep(uint8_t prev_ab, uint8_t curr_ab) {
    static const int8_t kTable[16] = {
        0,  1, -1,  0,
       -1,  0,  0,  1,
        1,  0,  0, -1,
        0, -1,  1,  0
    };
    return kTable[(prev_ab << 2) | curr_ab];
}

#ifdef ARDUINO
void encoderInit();
int  encoderGetTicks();    // akkumulierte Ticks seit letztem Aufruf, Reset nach Lesen
bool encoderGetPress();    // true einmalig pro Drück-Flanke
#endif
```

- [ ] **Schritt 2: Tests ausführen — müssen BESTEHEN**

```
pio test -e native
```

Erwartet:
```
test/native/test_encoder_logic/test_main.cpp:test_cw_full_cycle PASSED
test/native/test_encoder_logic/test_main.cpp:test_ccw_full_cycle PASSED
test/native/test_encoder_logic/test_main.cpp:test_no_change PASSED
OK (3 tests, 0 failures, 0 ignored)
```

- [ ] **Schritt 3: Commit**

```
git add src/encoder.h test/
git commit -m "feat: encoder quadrature decode logic with native unit tests"
```

---

## Chunk 3: Encoder-Modul (Arduino/ISR)

### Task 5: `encoder.cpp` implementieren

**Files:**
- Modify: `src/encoder.cpp`

Pins: A=45, B=42, Switch=41. ISR-Debounce für Switch via `millis()`.

- [ ] **Schritt 1: Implementierung schreiben**

```cpp
#include "encoder.h"
#include <Arduino.h>

#define PIN_A      45
#define PIN_B      42
#define PIN_SW     41
#define SW_DEBOUNCE_MS 50

static portMUX_TYPE sEncMux = portMUX_INITIALIZER_UNLOCKED;

static volatile int32_t sTicks    = 0;
static volatile bool    sPressFlag = false;
static uint8_t          sPrevAB   = 0;
static volatile uint32_t sLastPressMs = 0;

static void IRAM_ATTR isrEncoder() {
    uint8_t a = digitalRead(PIN_A);
    uint8_t b = digitalRead(PIN_B);
    uint8_t curr = (a << 1) | b;
    int8_t  delta = encoderQuadStep(sPrevAB, curr);
    sPrevAB = curr;
    if (delta != 0) {
        portENTER_CRITICAL_ISR(&sEncMux);
        sTicks += delta;
        portEXIT_CRITICAL_ISR(&sEncMux);
    }
}

static void IRAM_ATTR isrSwitch() {
    uint32_t now = millis();
    if (now - sLastPressMs >= SW_DEBOUNCE_MS) {
        sLastPressMs = now;
        portENTER_CRITICAL_ISR(&sEncMux);
        sPressFlag = true;
        portEXIT_CRITICAL_ISR(&sEncMux);
    }
}

void encoderInit() {
    pinMode(PIN_A,  INPUT_PULLUP);
    pinMode(PIN_B,  INPUT_PULLUP);
    pinMode(PIN_SW, INPUT_PULLUP);
    sPrevAB = (digitalRead(PIN_A) << 1) | digitalRead(PIN_B);
    attachInterrupt(digitalPinToInterrupt(PIN_A),  isrEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_B),  isrEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SW), isrSwitch,  FALLING);
}

int encoderGetTicks() {
    int32_t t;
    portENTER_CRITICAL(&sEncMux);
    t      = sTicks;
    sTicks = 0;
    portEXIT_CRITICAL(&sEncMux);
    return (int)t;
}

bool encoderGetPress() {
    bool p;
    portENTER_CRITICAL(&sEncMux);
    p          = sPressFlag;
    sPressFlag = false;
    portEXIT_CRITICAL(&sEncMux);
    return p;
}
```

- [ ] **Schritt 2: Kompilierung prüfen**

```
pio run -e crowpanel-s3
```

Erwartet: Keine Fehler oder Warnungen.

- [ ] **Schritt 3: Commit**

```
git add src/encoder.cpp
git commit -m "feat: encoder ISR with quadrature decode, debounced switch"
```

---

## Chunk 4: HID-Modul

### Task 6: `hid.h` und `hid.cpp` implementieren

**Files:**
- Modify: `src/hid.h`, `src/hid.cpp`

`USBHIDConsumerControl` ist Teil des ESP32 Arduino Core 3.x — keine externe Library nötig.

- [ ] **Schritt 1: `hid.h` schreiben**

```cpp
#pragma once

void hidInit();
void hidVolumeStep(int ticks);   // |ticks| HID-Reports; Vorzeichen = Richtung
void hidMuteToggle();
```

- [ ] **Schritt 2: `hid.cpp` schreiben**

```cpp
#include "hid.h"
#include "USB.h"
#include "USBHIDConsumerControl.h"

static USBHIDConsumerControl sConsumer;

void hidInit() {
    sConsumer.begin();   // registriert HID-Interface
    USB.begin();         // startet USB-Stack mit allen registrierten Interfaces (HID + CDC)
}

void hidVolumeStep(int ticks) {
    uint16_t key = (ticks > 0)
        ? CONSUMER_CONTROL_VOLUME_INCREMENT
        : CONSUMER_CONTROL_VOLUME_DECREMENT;
    int count = (ticks > 0) ? ticks : -ticks;
    for (int i = 0; i < count; i++) {
        sConsumer.press(key);
        sConsumer.release();
    }
}

void hidMuteToggle() {
    sConsumer.press(CONSUMER_CONTROL_MUTE);
    sConsumer.release();
}
```

- [ ] **Schritt 3: Kompilierung prüfen**

```
pio run -e crowpanel-s3
```

Erwartet: Keine Fehler. Falls `USBHIDConsumerControl.h not found` → pioarduino-Platform noch nicht vollständig heruntergeladen; nochmals `pio run` starten.

- [ ] **Schritt 4: Commit**

```
git add src/hid.h src/hid.cpp
git commit -m "feat: USB HID consumer control module (volume, mute)"
```

---

## Chunk 5: main.cpp + Flash + Hardware-Verifikation

### Task 7: `main.cpp` schreiben

**Files:**
- Modify: `src/main.cpp`

- [ ] **Schritt 1: `main.cpp` implementieren**

```cpp
#include <Arduino.h>
#include "encoder.h"
#include "hid.h"

void setup() {
    hidInit();                   // USB.begin() intern — muss VOR Serial stehen
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);    // kein Blockieren wenn kein Serial Monitor offen
    encoderInit();
    Serial.println("[BOOT] audioknubbel ready");
}

void loop() {
    int ticks = encoderGetTicks();
    if (ticks != 0) {
        Serial.printf("[ENC] ticks=%d\n", ticks);
        hidVolumeStep(ticks);
    }
    if (encoderGetPress()) {
        Serial.println("[ENC] press → mute");
        hidMuteToggle();
    }
    delay(10);
}
```

- [ ] **Schritt 2: Finaler Build**

```
pio run -e crowpanel-s3
```

Erwartet: Kompiliert ohne Fehler.

- [ ] **Schritt 3: Commit**

```
git add src/main.cpp
git commit -m "feat: main loop wiring encoder to USB HID, serial debug"
```

---

### Task 8: Auf das Board flashen

**Vorbedingung:** Board per USB-C angeschlossen, Treiber installiert.

> **Hinweis Download-Modus:** Da das CrowPanel keinen Bridge-Chip hat, muss der ESP32-S3 manuell in den Bootloader versetzt werden:
> 1. **BOOT**-Taste gedrückt halten
> 2. **RST**-Taste kurz drücken und loslassen
> 3. **BOOT** loslassen
> Das Board erscheint jetzt im Geräte-Manager als ESP32-S3 USB-Serial (nicht als HID).

- [ ] **Schritt 1: COM-Port ermitteln**

```
pio device list
```

Notierten COM-Port (z.B. `COM5`) in `platformio.ini` unter `[env:crowpanel-s3]` eintragen, falls PIO ihn nicht auto-detected:
```ini
upload_port = COM5
monitor_port = COM5
```

- [ ] **Schritt 2: Flashen**

```
pio run -e crowpanel-s3 -t upload
```

Nach erfolgreichem Upload: RST drücken (ohne BOOT) → Board bootet normal.

- [ ] **Schritt 3: Serial Monitor öffnen**

```
pio device monitor -e crowpanel-s3
```

Erwartet erste Zeile: `[BOOT] audioknubbel ready`

---

### Task 9: Hardware-Verifikation (Definition of Done)

- [ ] **Check 1:** Im Windows-Geräte-Manager unter „Menschliche Eingabegeräte" taucht ein neues HID-Gerät auf
- [ ] **Check 2:** Gleichzeitig erscheint unter „Anschlüsse (COM & LPT)" ein virtueller COM-Port
- [ ] **Check 3:** Encoder drehen → Windows-Lautstärke-OSD erscheint und ändert sich
- [ ] **Check 4:** Encoder drücken → Windows-Mute togglet (Lautsprecher-Icon in der Taskleiste)
- [ ] **Check 5:** Serial Monitor zeigt `[ENC] ticks=...` bei Drehung und `[ENC] press → mute` beim Drücken

---

## Nächste Milestones

- **Milestone 2:** GC9A01-Display-Init + LVGL 8.3.x + Volume-Arc-Widget → eigener Plan
- **Milestone 3:** CDC-Serial-Protokoll + C#-NAudio-Tray-App → eigener Plan
