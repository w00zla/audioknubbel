#pragma once
#include "USBCDC.h"

extern USBCDC USBSerial;

void hidInit();
void hidVolumeStep(int ticks);   // |ticks| HID-Reports; Vorzeichen = Richtung
void hidMuteToggle();

// Media-Transport (Consumer Control) — für Touch-Gesten.
void hidMediaPlayPause();
void hidMediaNext();
void hidMediaPrev();
