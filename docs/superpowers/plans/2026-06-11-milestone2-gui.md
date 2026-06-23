# Milestone 2 — Display + GUI Implementation Plan

> **⚠️ IMPLEMENTIERT 2026-06-11 (Opus) — Abweichungen vom Originalplan:**
> Ein früherer Versuch endete mit schwarzem Display. Ursachen + Fixes der umgesetzten Version:
> - **LovyanGFX `^1.2.0`** statt `^1.1.5` — 1.1.5 ist zu alt für Core 3.x (pioarduino 53.03.13), erzwang LEDC-Patches. 1.2.x kompiliert nativ.
> - **Backlight manuell** (`digitalWrite(46, HIGH)`), LovyanGFX `Light_PWM` entfernt → eliminiert das LEDC-Problem ganz.
> - **`spi_3wire = false`, 40 MHz** (alter Fehlversuch: true + 80 MHz → GC9A01 tot).
> - **Inkrementelles Bring-up** über `#define BRINGUP` in `main.cpp`: Stufe 1 = Backlight+Panel+R/G/B-Farbtest (kein LVGL), Stufe 2 = volle Arc-UI. Lokalisiert den Schwarz-Bildschirm-Fehler eindeutig.
> - **`LV_USE_ANIMIMG 0`** in `lv_conf.h` (Compile-Fix bleibt).
>
> Der Code unter `src/` ist die Quelle der Wahrheit; die Snippets unten sind der ursprüngliche Entwurf.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** GC9A01-Display über LovyanGFX initialisieren, LVGL 8.3.x aufsetzen, rundes Arc-Widget als Lautstärke-Anzeige mit Mute-Label auf dem CrowPanel 1.28" ESP32-S3.

**Architecture:** Drei neue Module (`display`, `ui`, `lv_conf.h`) ergänzen das bestehende `encoder`/`hid`-System. `display.cpp` besitzt die LovyanGFX-LGFX-Klasse und den LVGL-Flush-Callback. `ui.cpp` verwaltet alle LVGL-Widgets. `main.cpp` hält einen lokalen Volume-Counter (50 %–Start, ±2 % pro Tick) und ruft Display- und UI-Funktionen auf.

**Tech Stack:** ESP32-S3 / Arduino Framework (pioarduino), LovyanGFX ^1.1.5, LVGL ~8.3.11, PlatformIO

**Referenzen:**
- Elecrow Factory-Firmware (LVGL-Beispiel für dieses Board): [github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen](https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen)
- TasteTheCode-Empfehlung: bei Compile-Problemen ESP32-Core 2.0.10 + LovyanGFX 1.1.5 + LVGL 8.3.x pinnen

---

## Hardware-Kontext

```
Board:   CrowPanel 1.28" — ESP32-S3R8 (8 MB PSRAM, 16 MB Flash)
Display: GC9A01, 240×240 rund, SPI
         SCLK=10  MOSI=11  DC=3  CS=9  RST=14  BL=46
Encoder: A=45  B=42  SW=41   ← bereits fertig in encoder.h/cpp
```

---

## File Map

| File | Aktion | Verantwortung |
|------|--------|---------------|
| `platformio.ini` | modify | lib_deps + build_flags für LovyanGFX + LVGL |
| `src/lv_conf.h` | create | LVGL 8.3.x Konfiguration (Pflicht-Header) |
| `src/display.h` | create | API: `displayInit()` |
| `src/display.cpp` | create | LGFX-Klasse, LVGL-Buffer + Flush-Callback, Init |
| `src/ui.h` | create | API: `ui_init()`, `ui_set_volume(int)`, `ui_set_mute(bool)` |
| `src/ui.cpp` | create | LVGL Arc + Mute-Label, Style-Setup |
| `src/main.cpp` | modify | volume_pct-Counter, display/ui-Init, lv_task_handler() |

---

## Task 1: platformio.ini — Dependencies + Build Flags

**Files:**
- Modify: `platformio.ini`

- [ ] **Step 1: platformio.ini ersetzen**

```ini
[env:crowpanel-s3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.13/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_flags =
    -DARDUINO_USB_MODE=1
    -I src
    -DLV_CONF_INCLUDE_SIMPLE
lib_deps =
    lovyan03/LovyanGFX @ ^1.1.5
    lvgl/lvgl @ ~8.3.11
```

> **Warum `-DLV_CONF_INCLUDE_SIMPLE`?** LVGL sucht `lv_conf.h` standardmäßig zwei Verzeichnisse über sich selbst (`../../lv_conf.h` — relativ zum LVGL-Lib-Verzeichnis). Mit `LV_CONF_INCLUDE_SIMPLE` nutzt LVGL stattdessen `#include "lv_conf.h"`, und `-I src` macht `src/lv_conf.h` für den Compiler sichtbar.

- [ ] **Step 2: Commit**

```bash
git add platformio.ini
git commit -m "chore: add LovyanGFX + LVGL deps, build_flags for Milestone 2"
```

---

## Task 2: src/lv_conf.h — LVGL 8.3.x Konfiguration

**Files:**
- Create: `src/lv_conf.h`

- [ ] **Step 1: `src/lv_conf.h` erstellen**

> ⚠️ **Kritisch:** Das `#if 1` am Anfang MUSS `1` sein. Im LVGL-Template steht `#if 0` — dann ignoriert LVGL die gesamte Config und verwendet interne Defaults, was meist in einem Compile-Fehler endet.

```c
/* clang-format off */
#if 1 /* Set it to "1" to enable the content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0   /* auf 1 setzen wenn Farben falsch/invertiert wirken */
#define LV_COLOR_SCREEN_TRANSP  0
#define LV_COLOR_MIX_ROUND_OFS 0
#define LV_COLOR_CHROMA_KEY     lv_color_hex(0x00ff00)

/*=========================
   MEMORY SETTINGS
 *=========================*/
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE (48U * 1024U)   /* 48 KB interner LVGL-Heap */
    #define LV_MEM_ADR  0
    #if LV_MEM_ADR == 0
        #undef LV_MEM_POOL_INCLUDE
        #undef LV_MEM_POOL_ALLOC
        #undef LV_MEM_POOL_FREE
    #endif
#else
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD  30   /* ms */
#define LV_INDEV_DEF_READ_PERIOD 30   /* ms */

/* millis() als LVGL-Tick — kein manuelles lv_tick_inc() nötig */
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DPI_DEF 130

/*====================
 * DRAWING
 *====================*/
#define LV_DRAW_COMPLEX      1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_GRAD_CACHE_DEF_SIZE 0
#define LV_DITHER_GRADIENT    0
#define LV_DISP_ROT_MAX_BUF   (10*1024)

/*====================
 * GPU — alle aus
 *====================*/
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_SWM341_DMA   0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/*====================
 * LOGGING + ASSERTS
 *====================*/
#define LV_USE_LOG                  0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_ASSERT_HANDLER_INCLUDE   <stdint.h>
#define LV_ASSERT_HANDLER           while(1);

/*==================
 *   FONTS
 *==================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  0
#define LV_FONT_MONTSERRAT_12  0
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  0
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0
#define LV_FONT_DEFAULT        &lv_font_montserrat_14
#define LV_USE_FONT_SUBPX       1
#define LV_FONT_SUBPX_BGR       0
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_PLACEHOLDER 1

/*==================
 *  TEXT
 *==================*/
#define LV_TXT_ENC                          LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS                  " "
#define LV_TXT_LINE_BREAK_LONG_LEN          0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN  3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD                    "#"
#define LV_USE_BIDI                         0
#define LV_USE_ARABIC_PERSIAN_CHARS         0

/*==================
 * WIDGETS — nur Arc + Label
 *==================*/
#define LV_USE_ARC       1
#define LV_USE_BAR       0
#define LV_USE_BTN       0
#define LV_USE_BTNMATRIX 0
#define LV_USE_CANVAS    0
#define LV_USE_CHECKBOX  0
#define LV_USE_DROPDOWN  0
#define LV_USE_IMG       0
#define LV_USE_LABEL     1
#define LV_LABEL_TEXT_SELECTION  0
#define LV_LABEL_LONG_TXT_HINT   0
#define LV_USE_LINE      0
#define LV_USE_ROLLER    0
#define LV_USE_SLIDER    0
#define LV_USE_SWITCH    0
#define LV_USE_TEXTAREA  0
#define LV_USE_TABLE     0

/*==================
 * EXTRA WIDGETS — alle aus
 *==================*/
#define LV_USE_CALENDAR  0
#define LV_USE_CHART     0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN    0
#define LV_USE_KEYBOARD  0
#define LV_USE_LED       0
#define LV_USE_LIST      0
#define LV_USE_MENU      0
#define LV_USE_METER     0
#define LV_USE_MSGBOX    0
#define LV_USE_SPINBOX   0
#define LV_USE_SPINNER   0
#define LV_USE_TABVIEW   0
#define LV_USE_TILEVIEW  0
#define LV_USE_WIN       0
#define LV_USE_SPAN      0

/*==================
 * LAYOUTS
 *==================*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*==================
 * THEMES
 *==================*/
#define LV_USE_THEME_DEFAULT             1
#define LV_THEME_DEFAULT_DARK            1   /* dunkles Theme für schwarzen Hintergrund */
#define LV_THEME_DEFAULT_GROW            0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC               0
#define LV_USE_THEME_MONO                0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */
```

- [ ] **Step 2: Commit**

```bash
git add src/lv_conf.h
git commit -m "feat: add lv_conf.h — LVGL 8.3.x config for 240x240 dark theme"
```

---

## Task 3: display.h + display.cpp — LovyanGFX LGFX + LVGL Init

**Files:**
- Create: `src/display.h`
- Create: `src/display.cpp`

- [ ] **Step 1: `src/display.h` erstellen**

```cpp
#pragma once

void displayInit();
```

- [ ] **Step 2: `src/display.cpp` erstellen**

```cpp
#include "display.h"
#include <LovyanGFX.hpp>
#include <lvgl.h>

// ── LovyanGFX LGFX-Klasse für GC9A01 auf CrowPanel 1.28" ──────────────────
// Pins verifiziert gegen Elecrow-Schematic + Makerguides-Tutorial
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;
public:
    LGFX() {
        {
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_3wire   = false;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 10;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 3;
            cfg.freq_write  = 40000000;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs      = 9;
            cfg.pin_rst     = 14;
            cfg.pin_busy    = -1;
            cfg.readable    = false;
            cfg.invert      = true;    /* GC9A01 benötigt Color-Inversion */
            cfg.rgb_order   = false;
            cfg.dlen_16bit  = false;
            cfg.bus_shared  = false;
            cfg.panel_width  = 240;
            cfg.panel_height = 240;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = 46;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};

static LGFX               lcd;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lvgl_buf[240 * 10];  /* 10-Zeilen Partial-Buffer ≈ 4.8 KB */

// ── LVGL Flush-Callback: überträgt fertiges Tile per SPI ans Display ───────
static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels(reinterpret_cast<lgfx::rgb565_t*>(px), w * h);
    lcd.endWrite();
    lv_disp_flush_ready(drv);
}

// ── Öffentliche Init-Funktion ──────────────────────────────────────────────
void displayInit() {
    lcd.init();
    lcd.setRotation(0);
    lcd.setBrightness(200);   /* 0–255; 200 ≈ 78 % Helligkeit */

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, nullptr, 240 * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}
```

- [ ] **Step 3: Visueller Checkpoint nach Flash**

Erwartetes Ergebnis: **Display leuchtet weiß** (LVGL rendert noch nichts, Backlight aktiv).  
Falls Display dunkel bleibt → BL-Pin 46 prüfen, `lcd.setBrightness(255)` versuchen.  
Falls Compile-Fehler `writePixels` → LovyanGFX-Version prüfen (`^1.1.5`).

- [ ] **Step 4: Commit**

```bash
git add src/display.h src/display.cpp
git commit -m "feat: display module — LovyanGFX GC9A01 + LVGL flush callback"
```

---

## Task 4: ui.h + ui.cpp — LVGL Arc + Mute-Label

**Files:**
- Create: `src/ui.h`
- Create: `src/ui.cpp`

- [ ] **Step 1: `src/ui.h` erstellen**

```cpp
#pragma once
#include <stdbool.h>

void ui_init();
void ui_set_volume(int pct);   /* 0–100 */
void ui_set_mute(bool muted);
```

- [ ] **Step 2: `src/ui.cpp` erstellen**

```cpp
#include "ui.h"
#include <lvgl.h>

static lv_obj_t* s_arc;
static lv_obj_t* s_mute_label;

void ui_init() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    // Arc: 200×200, zentriert auf dem 240×240 runden Display
    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 200, 200);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 135);       /* Startpunkt unten-links (≈ 7 Uhr) */
    lv_arc_set_bg_angles(s_arc, 0, 270);   /* 270° Sweep — lässt unten Lücke */
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 50);           /* Startwert: 50% */
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);   /* Knob-Handle ausblenden */
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(s_arc, lv_color_make(0, 200, 255), LV_PART_INDICATOR);  /* cyan */
    lv_obj_set_style_arc_color(s_arc, lv_color_make(40, 40, 40),  LV_PART_MAIN);      /* dunkelgrau */
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_MAIN);

    // Mute-Label: zentriert, standardmäßig ausgeblendet
    s_mute_label = lv_label_create(scr);
    lv_label_set_text(s_mute_label, "MUTE");
    lv_obj_set_style_text_font(s_mute_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_mute_label, lv_color_make(255, 80, 80), 0);
    lv_obj_center(s_mute_label);
    lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_volume(int pct) {
    lv_arc_set_value(s_arc, pct);
}

void ui_set_mute(bool muted) {
    if (muted) {
        lv_obj_set_style_arc_color(s_arc, lv_color_make(80, 80, 80), LV_PART_INDICATOR);
        lv_obj_clear_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_arc_color(s_arc, lv_color_make(0, 200, 255), LV_PART_INDICATOR);
        lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
    }
}
```

- [ ] **Step 3: Visueller Checkpoint nach Flash**

Erwartetes Ergebnis:
- Schwarzer Hintergrund
- Cyan Arc-Ring (270°, von 7 Uhr nach 5 Uhr), zur Hälfte gefüllt (50%)
- Encoder drehen: Arc bewegt sich sofort
- Encoder drücken: Arc wird grau, "MUTE" erscheint rot in der Mitte
- Nochmal drücken: zurück zu cyan, Label weg

- [ ] **Step 4: Commit**

```bash
git add src/ui.h src/ui.cpp
git commit -m "feat: ui module — LVGL arc volume indicator + mute label"
```

---

## Task 5: main.cpp — Alles zusammenführen

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: `src/main.cpp` ersetzen**

```cpp
#include <Arduino.h>
#include "encoder.h"
#include "hid.h"
#include "display.h"
#include "ui.h"

static int  s_volume_pct = 50;    /* lokaler Schätzwert — HID kennt den echten Windows-Wert nicht */
static bool s_muted      = false;

void setup() {
    hidInit();
    USBSerial.setTxTimeoutMs(0);
    encoderInit();
    displayInit();
    ui_init();
    USBSerial.println("[BOOT] audioknubbel M2 ready");
}

void loop() {
    int ticks = encoderGetTicks();
    if (ticks != 0) {
        s_volume_pct = constrain(s_volume_pct - ticks * 2, 0, 100);
        ui_set_volume(s_volume_pct);
        hidVolumeStep(-ticks);
        USBSerial.printf("[ENC] vol=%d%%\n", s_volume_pct);
    }
    if (encoderGetPress()) {
        s_muted = !s_muted;
        ui_set_mute(s_muted);
        hidMuteToggle();
        USBSerial.printf("[ENC] mute=%d\n", (int)s_muted);
    }
    lv_task_handler();
    delay(10);
}
```

> **`- ticks * 2`:** Encoder CW = positive Ticks = Lautstärke runter aus User-Sicht? Falls die Drehrichtung falsch ist, Vorzeichen von `ticks * 2` umdrehen. `* 2` = 2 % pro Raststellung; anpassen falls zu grob/fein.

- [ ] **Step 2: Finaler Visueller Checkpoint nach Flash**

Erwartetes Ergebnis (Gesamtsystem):
- Boot: schwarzer Screen, Arc bei 50%
- Drehen CW: Arc nimmt ab, Windows-Lautstärke sinkt
- Drehen CCW: Arc nimmt zu, Windows-Lautstärke steigt
- Drücken: Arc grau + "MUTE" rot, Windows mutet
- Nochmal drücken: Arc cyan + "MUTE" weg, Windows unmutet
- USB HID + CDC parallel aktiv (wie in M1)

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: milestone 2 complete — display + LVGL arc wired to encoder + HID"
```

---

## Bekannte Stolpersteine

| Symptom | Lösung |
|---------|--------|
| Farben falsch / blass / invertiert | `LV_COLOR_16_SWAP 1` in `lv_conf.h` |
| Display bleibt schwarz nach Init | `lcd.setBrightness(255)` versuchen; BL-Pin 46 prüfen |
| LVGL findet `lv_conf.h` nicht | `-DLV_CONF_INCLUDE_SIMPLE` und `-I src` in `build_flags` prüfen |
| Compile-Fehler im lv_conf.h-Bereich | `#if 1` am Anfang der Datei sicherstellen (nicht `#if 0`) |
| Encoder reagiert, Display eingefroren | `lv_task_handler()` wird nicht in `loop()` aufgerufen |
| Drehrichtung Arc und Windows invers | Vorzeichen in `s_volume_pct - ticks * 2` umdrehen |
| Arc dreht falsch herum | `lv_arc_set_rotation` Winkel anpassen (aktuell 135°) |
