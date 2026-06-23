#include "display.h"
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>

#define PIN_BL    46   // Backlight (LEDC-PWM)
#define PIN_RING  40   // WS2812-Ring Power-Gate: HIGH = aus
#define PIN_PWR_1 1    // Display-/Backlight-Versorgung Rail 1: HIGH = an
#define PIN_PWR_2 2    // Display-/Backlight-Versorgung Rail 2: HIGH = an

// ── LovyanGFX LGFX-Klasse für GC9A01 auf CrowPanel 1.28" ──────────────────────
// 1:1 die funktionierende Config aus der Elecrow-Factory-Firmware
// (example/ESP32_Display_1_28). spi_3wire=true UND freq=80MHz sind hier korrekt
// (spi_3wire entfernt NICHT den DC-Pin, betrifft nur die Half-Duplex-Datenleitung).
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel;
    lgfx::Bus_SPI      _bus;
public:
    LGFX() {
        {
            auto cfg = _bus.config();
            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 80000000;
            cfg.freq_read   = 20000000;
            cfg.spi_3wire   = true;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 10;
            cfg.pin_mosi    = 11;
            cfg.pin_miso    = -1;
            cfg.pin_dc      = 3;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs          = 9;
            cfg.pin_rst         = 14;
            cfg.pin_busy        = -1;
            cfg.memory_width    = 240;
            cfg.memory_height   = 240;
            cfg.panel_width     = 240;
            cfg.panel_height    = 240;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable        = false;
            cfg.invert          = true;    /* GC9A01 benötigt Color-Inversion */
            cfg.rgb_order       = false;
            cfg.dlen_16bit      = false;
            cfg.bus_shared      = false;
            _panel.config(cfg);
        }
        setPanel(&_panel);
    }
};

static LGFX lcd;

// ── LVGL Draw-Buffer (Partial, 40 Zeilen ≈ 19 KB intern) ──────────────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lvgl_buf[240 * 40];

static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels(reinterpret_cast<lgfx::rgb565_t*>(px), w * h);
    lcd.endWrite();
    lv_disp_flush_ready(drv);
}

// ── Backlight via LEDC-PWM (5 kHz / 8 bit), Core-3.x pin-basierte API ──────────
// Statischer digitalWrite reicht hier nicht — der Treiber braucht das PWM-Signal.
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

void displayInit() {
    // KRITISCH: Power-Rails einschalten BEVOR das Panel initialisiert wird.
    // Ohne GPIO1/2=HIGH antwortet die GC9A01 zwar (init ok), zeigt aber nichts.
    pinMode(PIN_PWR_1, OUTPUT); digitalWrite(PIN_PWR_1, HIGH);
    pinMode(PIN_PWR_2, OUTPUT); digitalWrite(PIN_PWR_2, HIGH);
    pinMode(PIN_RING,  OUTPUT); digitalWrite(PIN_RING,  HIGH);   // Ring aus

    lcd.init();
    lcd.setRotation(0);
    lcd.fillScreen(lcd.color565(0, 0, 0));
    displayBacklightSet(true);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, nullptr, 240 * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}
