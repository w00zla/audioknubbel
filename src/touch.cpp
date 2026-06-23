#include "touch.h"
#include <Arduino.h>
#include <Wire.h>

#define PIN_SDA  6
#define PIN_SCL  7
#define PIN_RST  13

#define CST816_ADDR        0x15
#define REG_GESTURE        0x01   // [0]=Geste [1]=FingerNum [2..5]=x/y
#define REG_MOTIONMASK     0xEC   // bit0 EnDClick | bit1 EnConUD | bit2 EnConLR
#define REG_DISAUTOSLEEP   0xFE   // 1 = Auto-Sleep aus → Chip bleibt I²C-responsiv

static bool         sFingerPrev   = false;  // Finger im vorherigen Poll unten?
static bool         sPressEdge    = false;  // unkonsumierte Down-Flanke
static TouchGesture sGestureEvent = TG_NONE; // unkonsumierte fertige Geste

// Eine Geste gehört zu EINEM Druck-Zyklus. Der CST816D schreibt die Gesten-ID
// erst beim/nach dem Loslassen ins Register (finger=0) und lässt sie dort stehen
// (sie wird sonst tausendfach erneut gelesen). Darum pro Zyklus die zuletzt
// gemeldete Geste sammeln und nach einem kurzen Settle-Fenster nach dem Lift
// EINMAL ausgeben.
static bool         sCapturing    = false;  // Druck-Zyklus läuft (bis Finalize)
static bool         sLifted       = false;  // Finger in diesem Zyklus schon los?
static TouchGesture sCapture      = TG_NONE; // gesammelte Geste des Zyklus
static uint32_t     sSettleAt     = 0;
// Double-Click meldet der CST816D erst >80ms nach dem Lift -> max. so lange nach
// dem Lift auf eine (späte) Geste warten. Definitive Gesten (Swipe/Double-Tap)
// schließen sofort ab, daher bleibt das Fenster nur fürs Tap-/Leer-Ende relevant.
static const uint32_t SETTLE_MS   = 300;

// Liest count Bytes ab reg. false bei NACK/Busfehler (Chip antwortet z. B. nicht,
// wenn gerade nichts berührt wird) → Aufrufer behandelt das als „keine Berührung".
static bool readRegs(uint8_t reg, uint8_t* buf, size_t count) {
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;     // repeated start
    size_t got = Wire.requestFrom((int)CST816_ADDR, (int)count);
    if (got != count) return false;
    for (size_t i = 0; i < count; i++) buf[i] = Wire.read();
    return true;
}

void touchInit() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(10);
    digitalWrite(PIN_RST, HIGH);
    delay(50);                       // CST816 braucht nach Reset Bootzeit

    Wire.begin(PIN_SDA, PIN_SCL);

    // Auto-Sleep deaktivieren, damit Polling zuverlässig antwortet (USB-versorgt).
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(REG_DISAUTOSLEEP);
    Wire.write(0x01);
    Wire.endTransmission();

    // Gesten-Erkennung freischalten: Double-Click + kontinuierliches U/D + L/R.
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(REG_MOTIONMASK);
    Wire.write(0x07);
    Wire.endTransmission();
}

// Schließt den laufenden Druck-Zyklus ab und gibt die gesammelte Geste aus.
static void finalizeGesture() {
    if (sCapture != TG_NONE) sGestureEvent = sCapture;
    sCapturing = false;
    sLifted    = false;
    sCapture   = TG_NONE;
}

void touchPoll() {
    uint8_t d[6];
    bool         fingerNow = false;
    TouchGesture g         = TG_NONE;

    if (readRegs(REG_GESTURE, d, sizeof(d))) {
        g         = touchGestureFromReg(d[0]);
        fingerNow = (d[1] & 0x0F) > 0;          // FingerNum
    }

    if (fingerNow && !sFingerPrev) {            // neue Berührung
        if (sCapturing) finalizeGesture();      // vorherigen Zyklus zuerst abschließen
        sPressEdge = true;
        sCapturing = true;
        sLifted    = false;
        sCapture   = TG_NONE;
    }

    if (sCapturing) {
        if (g != TG_NONE) sCapture = g;         // last-wins (während UND nach Lift)
        if (!fingerNow && sFingerPrev) {        // Lift-Flanke -> Settle-Fenster starten
            sLifted   = true;
            sSettleAt = millis() + SETTLE_MS;
        }
        if (sLifted && (touchGestureIsDefinitive(sCapture) ||
                        (int32_t)(millis() - sSettleAt) >= 0)) {
            finalizeGesture();
        }
    }

    sFingerPrev = fingerNow;
}

bool touchTakePress() {
    bool e = sPressEdge;
    sPressEdge = false;
    return e;
}

TouchGesture touchTakeGesture() {
    TouchGesture g = sGestureEvent;
    sGestureEvent = TG_NONE;
    return g;
}

void touchCancelGesture() {
    sPressEdge    = false;
    sGestureEvent = TG_NONE;
    sCapturing    = false;
    sLifted       = false;
    sCapture      = TG_NONE;
}
