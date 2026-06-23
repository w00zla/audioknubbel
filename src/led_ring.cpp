#include "led_ring.h"
#include <Arduino.h>
#include "esp32-hal-rmt.h"

static const uint8_t PIN_RING_POWER = 40;   // WS2812 power gate: LOW = on
static const uint8_t PIN_RING_DATA  = 48;
static const uint8_t LED_COUNT      = 5;
static const uint8_t MUTE_RED       = 2;    // Testwert zwischen "nur eine LED" und "ring-tauglich"

static bool s_rmt_ready = false;

static void append_byte(rmt_data_t* data, int& index, uint8_t value) {
    for (int bit = 7; bit >= 0; bit--) {
        bool one = (value & (1 << bit)) != 0;
        data[index].level0 = 1;
        data[index].duration0 = one ? 8 : 4;   // 10 MHz RMT tick: 0.8 us / 0.4 us
        data[index].level1 = 0;
        data[index].duration1 = one ? 4 : 8;   // 0.4 us / 0.8 us
        index++;
    }
}

static void write_color(uint8_t red, uint8_t green, uint8_t blue) {
    if (!s_rmt_ready) return;

    rmt_data_t data[LED_COUNT * 24];
    int index = 0;
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        append_byte(data, index, green);   // WS2812B order: GRB
        append_byte(data, index, red);
        append_byte(data, index, blue);
    }
    rmtWrite(PIN_RING_DATA, data, index, RMT_WAIT_FOR_EVER);
}

void ledRingInit() {
    pinMode(PIN_RING_POWER, OUTPUT);
    pinMode(PIN_RING_DATA, OUTPUT);
    digitalWrite(PIN_RING_DATA, LOW);
    digitalWrite(PIN_RING_POWER, HIGH);
    s_rmt_ready = rmtInit(PIN_RING_DATA, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
}

void ledRingSetMute(bool muted) {
    if (muted) {
        digitalWrite(PIN_RING_POWER, LOW);
        delayMicroseconds(300);
        write_color(MUTE_RED, 0, 0);
    } else {
        write_color(0, 0, 0);
        digitalWrite(PIN_RING_POWER, HIGH);
    }
}
