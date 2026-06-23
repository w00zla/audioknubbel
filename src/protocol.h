#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum class ProtoCmd : uint8_t { None, SetVolume, SetMute, Identify, Ping, EnterBoot, SetBrightness, QueryBrightness, SetStandby, QueryStandby };

struct ProtoResult {
    ProtoCmd cmd;
    int      value;   // SetVolume: 0..100 (geclampt); SetMute: 0/1
};

// Pure line parser — kein Arduino, header-only (wie encoderQuadStep).
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
    } else if (strncmp(line, "BRIGHT?", 7) == 0) {
        r.cmd = ProtoCmd::QueryBrightness;
    } else if (strncmp(line, "BRIGHT:", 7) == 0) {
        int v = atoi(line + 7);
        if (v < 5)   v = 5;     // 5 % ist Minimum — „aus" macht der Standby
        if (v > 100) v = 100;
        r.cmd = ProtoCmd::SetBrightness;
        r.value = v;
    } else if (strncmp(line, "STBY?", 5) == 0) {
        r.cmd = ProtoCmd::QueryStandby;
    } else if (strncmp(line, "STBY:", 5) == 0) {
        int v = atoi(line + 5);
        if (v < 5)  v = 5;
        if (v > 60) v = 60;
        r.cmd = ProtoCmd::SetStandby;
        r.value = v;
    } else if (strncmp(line, "ID?", 3) == 0) {
        r.cmd = ProtoCmd::Identify;
    } else if (strncmp(line, "PING", 4) == 0) {
        r.cmd = ProtoCmd::Ping;   // Heartbeat — nur Lebenszeichen, kein State
    } else if (strncmp(line, "BOOT?", 5) == 0) {
        r.cmd = ProtoCmd::EnterBoot;   // App kündigt Bootloader-Reset an
    }
    return r;
}

#ifdef ARDUINO
void protocolPoll();              // liest USBSerial, wendet Kommandos an (siehe protocol.cpp)
bool protocolCompanionConnected(); // true, solange zuletzt < Timeout eine Zeile kam
#endif
