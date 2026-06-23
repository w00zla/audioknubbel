#pragma once

#ifdef ARDUINO
// NVS-gestützter Standby-Timeout in Sekunden (5..60). standbyCfgInit() lädt den
// Wert (Default 15). standbyCfgSet() clamped + speichert. standbyCfgGet() liefert
// den aktuellen Wert (für STBY?-Antwort und den Standby-Check in main.cpp).
void standbyCfgInit();
void standbyCfgSet(int sec);
int  standbyCfgGet();
#endif
