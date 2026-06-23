#pragma once

#ifdef ARDUINO
// NVS-gestützter Theme-Index. themeCfgInit() lädt den Wert (Default 0).
// themeCfgSet() clamped auf >=0 + speichert. themeCfgGet() liefert den rohen Wert;
// der Aufrufer clamped gegen ui_theme_count(), damit theme_cfg nicht an die
// Theme-Registry koppelt (analog standby_cfg).
void themeCfgInit();
void themeCfgSet(int index);
int  themeCfgGet();
#endif
