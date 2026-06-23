# Design: Volume-%-Anzeige auf dem Knob

**Datum:** 2026-06-13
**Status:** Approved (Design)
**Scope:** Board-UI (`src/ui.*`, `src/lv_conf.h`)

## Ziel

Das runde GC9A01-Display zeigt aktuell nur den LVGL-Arc (cyan, 0–100, 270°-Sweep)
und ein verstecktes „MUTE"-Label. Es soll zusätzlich eine **numerische
Lautstärke-Anzeige** in der Arc-Mitte erscheinen, z.B. `50%`.

## Anforderungen

- Zentrierte Zahl im Format `"<pct>%"` (z.B. `50%`), Werte 0–100.
- Font **Montserrat 48**, Farbe **weiß** (Arc bleibt cyan).
- Bei Mute: Zahl wird durch das vorhandene „MUTE"-Label ersetzt (Zahl ausgeblendet).
- Initialwert `50%` (passend zum Arc-Startwert 50).

## Architektur / Änderungen

Reine LVGL-Display-Sache, keine pure Logik betroffen. Verifikation **on-device**
(kein Host-gcc auf dieser Maschine).

### `src/lv_conf.h`
- `LV_FONT_MONTSERRAT_48` von `0` auf `1` setzen.
  Kostet Flash für die Glyphen (mind. Ziffern 0–9 und `%`).

### `src/ui.cpp`
- Neues statisches Label `s_vol_label`:
  - erstellt auf `lv_scr_act()`, Font `&lv_font_montserrat_48`,
    Textfarbe weiß, `lv_obj_center()`.
  - Initialtext `"50%"`.
- `ui_set_volume(int pct)`:
  - setzt weiterhin den Arc-Wert (`lv_arc_set_value`),
  - **zusätzlich** Label-Text via `lv_label_set_text_fmt(s_vol_label, "%d%%", pct)`.
- `ui_set_mute(bool muted)`:
  - `muted == true`: `s_vol_label` ausblenden (`LV_OBJ_FLAG_HIDDEN`),
    `s_mute_label` einblenden, Arc grau (wie heute).
  - `muted == false`: `s_mute_label` ausblenden, `s_vol_label` einblenden,
    Arc cyan (wie heute).

### `src/ui.h`
- Keine API-Änderung. `ui_init()`, `ui_set_volume(int)`, `ui_set_mute(bool)`
  bleiben unverändert.

## Layout-Validierung

Der breiteste Fall `100%` ist bei Montserrat 48 ~116px breit und passt in den
inneren Arc-Durchmesser (200 − 2×12 = 176px). Höhe 48px unkritisch.

## Verhalten / Datenfluss

```
Encoder/CDC → main.cpp → ui_set_volume(pct) → Arc-Wert + "<pct>%"-Label
                       → ui_set_mute(true)   → Label aus, "MUTE" an, Arc grau
                       → ui_set_mute(false)  → "MUTE" aus, Label an, Arc cyan
```

Graceful Degradation bleibt unberührt: lokaler Schätzwert speist `ui_set_volume`
weiterhin auch ohne laufende Companion-App.

## Out of Scope

- Keine Animationen/Übergänge für die Zahl.
- Keine zusätzlichen Companion-/Protokoll-Änderungen.
- Keine Änderungen an `display.cpp` oder GPIO (Hardware-Tabu).

## Testplan (on-device)

1. Build `pio.exe run -e crowpanel-s3` ohne Fehler.
2. Nach Flash: Display zeigt `50%` zentriert beim Start.
3. Encoder drehen → Zahl ändert sich synchron zum Arc, clamped 0–100.
4. Mute auslösen → Zahl verschwindet, „MUTE" erscheint, Arc grau.
5. Unmute → Zahl wieder da, Arc cyan.
