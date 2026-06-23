#include "encoder.h"
#include <Arduino.h>

#define PIN_A          45
#define PIN_B          42
#define PIN_SW         41
#define SW_DEBOUNCE_MS 50

static portMUX_TYPE sEncMux = portMUX_INITIALIZER_UNLOCKED;

static volatile int32_t  sTicks       = 0;
static volatile bool     sPressFlag   = false;
static volatile uint32_t sLastPressMs = 0;
static uint8_t           sPrevAB      = 0;

static void IRAM_ATTR isrEncoder() {
    uint8_t a    = digitalRead(PIN_A);
    uint8_t b    = digitalRead(PIN_B);
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
