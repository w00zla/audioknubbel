#include "protocol.h"
#ifdef ARDUINO
#include <Arduino.h>
#include "hid.h"   // USBSerial
#include "brightness.h"   // brightnessGet() für die BRIGHT?-Antwort
#include "standby_cfg.h"  // standbyCfgGet() für die STBY?-Antwort

// In main.cpp definiert — hält lokalen Schätzwert + Realwert konsistent.
extern void protocolApplyVolume(int pct);
extern void protocolApplyMute(bool muted);
extern void protocolApplyBrightness(int pct);   // in main.cpp: setzen + speichern + wecken
extern void protocolApplyStandby(int sec);      // in main.cpp: setzen + speichern + wecken
extern void protocolEnterBoot();   // startet die Countdown-State-Machine in main.cpp

// Companion gilt als verbunden, solange innerhalb dieses Fensters eine Zeile kam.
static const unsigned long CONNECT_TIMEOUT_MS = 3000;

static char          s_buf[64];
static uint8_t       s_len      = 0;
static bool          s_everSeen = false;   // bis zum ersten Lebenszeichen: getrennt
static unsigned long s_lastSeen = 0;

static void handleLine(const char* line) {
    ProtoResult r = protocolParseLine(line);
    switch (r.cmd) {
        case ProtoCmd::SetVolume: protocolApplyVolume(r.value);        break;
        case ProtoCmd::SetMute:   protocolApplyMute(r.value != 0);     break;
        case ProtoCmd::Identify:  USBSerial.println("AUDIOKNUBBEL M3");    break;
        case ProtoCmd::Ping:                                           break;
        case ProtoCmd::EnterBoot: protocolEnterBoot();                 break;
        case ProtoCmd::SetBrightness:   protocolApplyBrightness(r.value);     break;
        case ProtoCmd::QueryBrightness: {
            char buf[16];
            snprintf(buf, sizeof(buf), "BRIGHT:%d", brightnessGet());
            USBSerial.println(buf);
            break;
        }
        case ProtoCmd::SetStandby:      protocolApplyStandby(r.value);        break;
        case ProtoCmd::QueryStandby: {
            char buf[16];
            snprintf(buf, sizeof(buf), "STBY:%d", standbyCfgGet());
            USBSerial.println(buf);
            break;
        }
        case ProtoCmd::None:                                           break;
    }
}

bool protocolCompanionConnected() {
    return s_everSeen && (millis() - s_lastSeen) <= CONNECT_TIMEOUT_MS;
}

void protocolPoll() {
    while (USBSerial.available() > 0) {
        char c = (char)USBSerial.read();
        if (c == '\n') {
            s_buf[s_len] = '\0';
            // Jede nicht-leere Zeile (PING, VOL, MUTE, …) zählt als Lebenszeichen.
            if (s_len > 0) { s_everSeen = true; s_lastSeen = millis(); }
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
