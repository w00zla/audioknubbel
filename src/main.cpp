#include <Arduino.h>
#include <lvgl.h>
#include "encoder.h"
#include "hid.h"
#include "display.h"
#include "ui.h"
#include "protocol.h"
#include "touch.h"
#include "brightness.h"
#include "standby_cfg.h"
#include "theme_cfg.h"

// usb_persist_restart() aus dem Arduino-Core (esp32-hal-tinyusb.h). Direkt
// deklariert, um nicht das komplette TinyUSB-Header in diese TU zu ziehen; die
// enum-Reihenfolge entspricht dem Core. RESTART_BOOTLOADER schaltet auf den
// ROM USB-Serial/JTAG um und resettet — exakt der Pfad, den auch der 1200-Baud-
// Touch nimmt (USBCDC.cpp), nur hier deterministisch aus der Firmware ausgelöst.
extern "C" {
    typedef enum {
        RESTART_NO_PERSIST, RESTART_PERSIST, RESTART_BOOTLOADER,
        RESTART_BOOTLOADER_DFU, RESTART_TYPE_MAX
    } restart_type_t;
    void usb_persist_restart(restart_type_t mode);
}

// Lokaler Lautstärke-Schätzwert (Fallback, wenn die Companion-App nicht läuft).
// HID kennt den echten Windows-Wert nicht; wir zählen selbst mit (Start 50 %,
// ±2 % pro Encoder-Raststellung). Die App überschreibt ihn via protocolApply*.
static int  s_volume_pct = 50;
static bool s_muted      = false;

// ── Standby ───────────────────────────────────────────────────────────────
// Nach dem Standby-Timeout ohne Aktivität geht das Backlight komplett aus.
// Aktivität = Encoder-Dreh/-Druck ODER ein neuer Volume/Mute-Push der App.
// Der Timeout wird aus NVS gelesen (standby_cfg, Sekunden) und unten in ms genutzt.
static uint32_t s_last_activity_ms = 0;
static bool     s_standby          = false;

// ── Flash-Countdown ─────────────────────────────────────────────────────────
// Auf "BOOT?" der App spielt das Board 5 s einen Countdown ab und meldet danach
// einmal "BOOTREADY"; die App löst daraufhin den 1200-Baud-Bootloader-Reset aus.
static const uint32_t BOOT_COUNTDOWN_MS = 5000;
static bool     s_boot_pending    = false;
static uint32_t s_boot_deadline   = 0;
static int      s_boot_last_shown = -1;

// ── Theme-Switcher ──────────────────────────────────────────────────────────
// Long-Press öffnet das Overlay; im offenen Zustand übernimmt der Switcher alle
// Eingaben: Dreh = Live-Vorschau, Long-Tap = übernehmen + NVS-Speichern,
// Single-Tap = abbrechen (zurück auf s_theme_original). Der Encoder ist mechanisch
// sehr sensibel (mehrere Ticks pro Raststellung) — daher zählt im Switcher nur die
// Drehrichtung, und ein Cooldown fasst den Tick-Burst einer Raststellung zu einem
// Schritt zusammen.
static const uint32_t SW_SCROLL_COOLDOWN_MS = 150;
static bool     s_switcher       = false;
static int      s_theme_original = 0;
static uint32_t s_sw_last_scroll = 0;

static void wakeUp() {
    if (s_standby) {
        s_standby = false;
        displayBacklightSet(true);
    }
    s_last_activity_ms = millis();
}

// Vom Serial-Parser (protocol.cpp) aufgerufen, wenn die App den echten State pusht.
void protocolApplyVolume(int pct) {
    s_volume_pct = constrain(pct, 0, 100);
    ui_set_volume(s_volume_pct);
    wakeUp();
}
void protocolApplyMute(bool muted) {
    s_muted = muted;
    ui_set_mute(s_muted);
    wakeUp();
}

// Vom Serial-Parser auf "BRIGHT:<n>" aufgerufen: Helligkeit setzen + persistieren.
// Weckt das Board, damit die Änderung sichtbar wird.
void protocolApplyBrightness(int pct) {
    brightnessSet(pct);
    wakeUp();
}

// Vom Serial-Parser auf "STBY:<n>" aufgerufen: Standby-Timeout (Sekunden) setzen +
// persistieren. wakeUp() setzt den Aktivitäts-Timer zurück, damit die neue Schwelle
// ab jetzt frisch zählt.
void protocolApplyStandby(int sec) {
    standbyCfgSet(sec);
    wakeUp();
}

// Vom Serial-Parser auf "BOOT?" aufgerufen: startet die nicht-blockierende
// Countdown-State-Machine (siehe loop()). Doppelte BOOT? ignorieren.
void protocolEnterBoot() {
    if (s_boot_pending) return;
    wakeUp();
    s_boot_pending    = true;
    s_boot_deadline   = millis() + BOOT_COUNTDOWN_MS;
    s_boot_last_shown = -1;
}

void setup() {
    hidInit();                       // USB HID (Consumer Control) + CDC
    USBSerial.setTxTimeoutMs(0);     // nicht blockieren ohne offenen Serial-Monitor
    encoderInit();
    displayInit();                   // Power-Rails, GC9A01, Backlight, LVGL
    brightnessInit();                // Helligkeit aus NVS laden + aufs Backlight anwenden
    standbyCfgInit();                // Standby-Timeout aus NVS laden
    themeCfgInit();                  // gespeicherten Theme-Index aus NVS laden
    touchInit();                     // CST816D via I²C (nur Wake aus Standby)
    ui_init();                       // Arc + Mute-Label
    {                                // gespeichertes Theme anwenden (gegen Count geclampt)
        int t = themeCfgGet();
        if (t < 0 || t >= ui_theme_count()) t = 0;
        ui_theme_set(t);
    }
    s_last_activity_ms = millis();   // Standby-Timer starten
    USBSerial.println("[BOOT] audioknubbel M3 ready");
}

void loop() {
    int  ticks   = encoderGetTicks();
    bool pressed = encoderGetPress();
    touchPoll();                           // ein I²C-Read; Events danach abholen
    bool         touched = touchTakePress();
    TouchGesture gesture = touchTakeGesture();

    // Der mechanische Encoder-Press ist Mute. Falls der Druck gleichzeitig den
    // Touch-Sensor triggert, darf daraus keine Play/Pause-Geste entstehen.
    if (pressed) {
        touchCancelGesture();
        touched = false;
        gesture = TG_NONE;
    }

    // Während des Flash-Countdowns alle Eingaben verwerfen — kein Volume-Sprung,
    // Mute-Toggle oder Track-Wechsel kurz vor dem Reset.
    if (s_boot_pending) {
        ticks = 0;
        pressed = false;
        touched = false;
        gesture = TG_NONE;
    }

    // Im Standby weckt jede Eingabe den Bildschirm und wird konsumiert, damit das
    // Aufwachen keinen Volume-Sprung / Track-Wechsel auslöst. AUSNAHME: der
    // Encoder-Press ist eine eindeutige, bewusste Aktion — er darf wecken UND
    // muten (fällt unten zur Mute-Logik durch), sonst kann man aus dem Standby
    // nicht direkt stummschalten.
    if (s_standby && (ticks != 0 || pressed || touched || gesture != TG_NONE)) {
        wakeUp();
        ticks = 0;
        gesture = TG_NONE;
        // pressed bewusst NICHT zurücksetzen -> Wake + Mute in einem Druck.
    }

    // Theme-Switcher: Long-Press öffnet das Overlay. Im offenen Zustand übernimmt
    // der Switcher alle Eingaben (Dreh = Live-Vorschau, Druck = übernehmen,
    // Tap/Long-Press = abbrechen) und verschluckt sie für die übrige Loop.
    if (!s_switcher && gesture == TG_LONG_PRESS) {
        s_switcher       = true;
        s_theme_original = ui_theme_get();
        ui_switcher_open();
        gesture = TG_NONE;
        s_last_activity_ms = millis();
    } else if (s_switcher) {
        if (ticks != 0) {                                // ein Detent = ein Schritt:
            uint32_t now = millis();                     // nur Richtung, Burst per Cooldown
            if (now - s_sw_last_scroll >= SW_SCROLL_COOLDOWN_MS) {
                ui_switcher_scroll(ticks > 0 ? 1 : -1);
                s_sw_last_scroll = now;
            }
        }
        if (gesture == TG_LONG_PRESS) {                  // Long-Tap = übernehmen + persistieren
            int sel = ui_switcher_selected();
            themeCfgSet(sel);
            USBSerial.printf("THEME:%s\n", ui_theme_name(sel));
            ui_switcher_close();
            s_switcher = false;
        } else if (gesture == TG_TAP) {                  // Single-Tap = abbrechen auf Original
            ui_theme_set(s_theme_original);
            ui_switcher_close();
            s_switcher = false;
        }
        ticks = 0;
        pressed = false;                                 // Encoder-Druck im Switcher: ohne Funktion
        gesture = TG_NONE;
        s_last_activity_ms = millis();                   // bleibt wach beim Blättern
    }

    // Media-Gesten nur im wachen Zustand. Der CST816D meldet Double-Click nativ
    // als TG_DOUBLE_TAP (Reg 0xB). Single-Tap bleibt wach ohne Funktion.
    switch (gesture) {
        case TG_SWIPE_LEFT:
            hidMediaPrev();
            ui_flash_media(UI_MEDIA_PREV);
            break;
        case TG_SWIPE_RIGHT:
            hidMediaNext();
            ui_flash_media(UI_MEDIA_NEXT);
            break;
        case TG_DOUBLE_TAP:
            hidMediaPlayPause();
            ui_flash_media(UI_MEDIA_PLAY_PAUSE);
            break;
        default: break;
    }
    if (touched || gesture != TG_NONE) s_last_activity_ms = millis();  // hält wach

    if (ticks != 0) {
        s_volume_pct = constrain(s_volume_pct - ticks * 2, 0, 100);
        ui_set_volume(s_volume_pct);
        hidVolumeStep(-ticks);
        s_last_activity_ms = millis();
    }
    if (pressed) {
        s_muted = !s_muted;
        ui_set_mute(s_muted);
        hidMuteToggle();
        s_last_activity_ms = millis();
    }
    protocolPoll();                  // App pusht echten State -> protocolApply* (weckt auf)

    // Flash-Countdown abspulen: jede Sekunde das Overlay aktualisieren, am Ende
    // einmal BOOTREADY senden. Nicht-blockierend, damit LVGL/Heartbeat weiterlaufen.
    if (s_boot_pending) {
        s_last_activity_ms = millis();          // kein Standby während des Countdowns
        uint32_t now = millis();
        int remaining = (now >= s_boot_deadline)
            ? 0 : (int)((s_boot_deadline - now + 999) / 1000);
        if (remaining != s_boot_last_shown) {
            s_boot_last_shown = remaining;
            ui_boot_countdown(remaining);
        }
        if (now >= s_boot_deadline) {
            // Klaren Endframe rendern, damit der eingefrorene Bootloader-Screen
            // "→ FLASH" zeigt statt der letzten Countdown-Ziffer. Mehrfach pumpen,
            // damit LVGL invalidiert + aufs Panel flusht, bevor wir resetten.
            ui_boot_flashing();
            for (int k = 0; k < 3; k++) { lv_timer_handler(); delay(20); }

            // Bestätigung an die App (rein informativ fürs UI), kurz rausschreiben
            // lassen, dann den ROM-Download-Modus DETERMINISTISCH selbst auslösen.
            // Kein Host-1200-Touch mehr — der ist auf Native-USB nichtdeterministisch.
            USBSerial.println("BOOTREADY");
            USBSerial.flush();
            delay(150);                              // BOOTREADY zum Host drainen lassen
            usb_persist_restart(RESTART_BOOTLOADER); // kehrt nicht zurück (esp_restart)
            s_boot_pending = false;                  // Fallback, falls der Reset doch scheitert
        }
    }

    ui_set_connected(protocolCompanionConnected());  // roter Punkt bei Disconnect

    // Nach Inaktivität ins Standby: Backlight komplett aus.
    if (!s_standby && (millis() - s_last_activity_ms >= (uint32_t)standbyCfgGet() * 1000UL)) {
        s_standby = true;
        displayBacklightSet(false);
    }

    lv_timer_handler();              // LVGL rendern — muss regelmäßig laufen
    delay(5);
}
