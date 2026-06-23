# Media-Gesten — Design

**Stand:** 2026-06-14 · **Branch:** `feature/media-gestures`
**Ziel:** Drei Touch-Gesten steuern Medienwiedergabe via HID:
Double-Tap = Play/Pause, Swipe-Left = Previous Track, Swipe-Right = Next Track.

## Kontext

Der Touch-Treiber (`touch.h/.cpp`) liest bereits die volle CST816-Gesten-ID
(Register 0x01) und hat sie für genau diesen Zweck als Fundament exponiert.
`USBHIDConsumerControl` (`hid.cpp`) kann neben Volume/Mute auch Media-Keys.
Kein neuer USB-Stack, keine neue Dependency.

## HID (`hid.h/.cpp`)

Drei Funktionen analog zu `hidMuteToggle()`:
- `hidMediaPlayPause()` → `CONSUMER_CONTROL_PLAY_PAUSE`
- `hidMediaNext()`      → `CONSUMER_CONTROL_SCAN_NEXT_TRACK`
- `hidMediaPrev()`      → `CONSUMER_CONTROL_SCAN_PREVIOUS_TRACK`

## Touch (`touch.h/.cpp`) — Refactor auf einen Read/Loop

Bisher las `touchGetPress()` den Chip. Neu: ein expliziter Poll + zwei Consumer,
damit Press-Flanke und Geste denselben einen I²C-Read teilen:

| Funktion | Zweck |
|---|---|
| `void touchPoll()` | Liest den Chip einmal pro Loop, aktualisiert Press-/Gesten-State. |
| `bool touchTakePress()` | Finger-Down-Flanke (Wake), konsumiert. (= bisheriges Verhalten) |
| `TouchGesture touchTakeGesture()` | Abgeschlossene Geste **beim Finger-Lift**, einmal, konsumiert; sonst `TG_NONE`. Ersetzt `touchLastGesture()`. |

Geste wird beim Übergang Finger-unten→oben anhand der zuletzt gemeldeten
Gesten-ID ausgegeben (entprellt). Der evtl. „erste Tap" eines Doppeltipps liefert
höchstens einen Single-Tap — der ist im wachen Zustand ohnehin no-op.

## Integration `main.cpp`

```
touchPoll();
bool touched = touchTakePress();
TouchGesture g = touchTakeGesture();

if (s_standby) {
    // Aufwachen konsumiert ALLES -> Geste löst kein Media aus (kein Versehen)
    if (ticks||pressed||touched|| g!=TG_NONE) { wakeUp(); ticks=0; pressed=false; }
} else {
    switch (g) {                       // Media nur im wachen Zustand
        case TG_DOUBLE_TAP:  hidMediaPlayPause(); break;
        case TG_SWIPE_LEFT:  hidMediaPrev();      break;
        case TG_SWIPE_RIGHT: hidMediaNext();      break;
        default: break;
    }
    if (touched || g != TG_NONE) s_last_activity_ms = millis();  // hält wach
}
```

## Bewusste Entscheidungen

- **Gesten wirken nur wach.** Im Standby weckt jede Berührung nur (konsumiert),
  damit ein Aufweck-Wisch nicht versehentlich den Track wechselt.
- **Kein Display-Feedback** (User-Entscheid) — Bestätigung kommt über die Musik.
- Single-Tap wach = no-op (kein Konflikt mit Double-Tap).

## Risiko / on-device zu verifizieren

Die Wisch-Richtung des CST816 (`SWIPE_LEFT`=0x03 / `RIGHT`=0x04) hängt von der
Panel-Orientierung ab und ist ggf. gegenüber der gefühlten Richtung gedreht.
Falls links/rechts vertauscht: die beiden `case`-Zweige tauschen. Beim Flash-Test
klären (Serial-Print der erkannten Geste hilft).

## Verifizierung

Build `pio run -e crowpanel-s3`. On-device: Double-Tap/Swipes testen. Flash nach „go".
Companion-COM-Fix ist firmware-unabhängig (kein erneutes Flashen dafür nötig).
