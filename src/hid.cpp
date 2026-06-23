#include "hid.h"
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDConsumerControl.h"

USBCDC USBSerial;
static USBHIDConsumerControl sConsumer;

void hidInit() {
    sConsumer.begin();
    USBSerial.begin();
    USB.productName("audioknubbel");        // Discovery-Tiebreaker für die Companion-App
    USB.manufacturerName("audioknubbel");
    USB.begin();
}

void hidVolumeStep(int ticks) {
    uint16_t key = (ticks > 0)
        ? CONSUMER_CONTROL_VOLUME_INCREMENT
        : CONSUMER_CONTROL_VOLUME_DECREMENT;
    int count = (ticks > 0) ? ticks : -ticks;
    for (int i = 0; i < count; i++) {
        sConsumer.press(key);
        sConsumer.release();
    }
}

void hidMuteToggle() {
    sConsumer.press(CONSUMER_CONTROL_MUTE);
    sConsumer.release();
}

void hidMediaPlayPause() {
    sConsumer.press(CONSUMER_CONTROL_PLAY_PAUSE);
    sConsumer.release();
}

void hidMediaNext() {
    sConsumer.press(CONSUMER_CONTROL_SCAN_NEXT);
    sConsumer.release();
}

void hidMediaPrev() {
    sConsumer.press(CONSUMER_CONTROL_SCAN_PREVIOUS);
    sConsumer.release();
}
