#pragma once
#include <stdint.h>

// Pure quadrature decode — kein Arduino, daher native-testbar.
// prev_ab: vorheriger Zustand (A<<1|B), curr_ab: aktueller Zustand.
// Gibt +1 (CW), -1 (CCW) oder 0 (keine/ungueltige Aenderung) zurueck.
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
bool encoderGetPress();    // true einmalig pro Drueck-Flanke
#endif
