# Handoff — UI: Vol-Arc & Label verkleinert (2026-06-15)

## Was umgesetzt wurde

Reine UI-Größenanpassung am Volume-Screen, nach Augenmaß-Feedback:

- **Vol-Arc (Tick-Ring) eine Stufe kleiner:** `R_OUT 64→60`, `R_IN 53→50`
  (Ringbreite bleibt ~10px, Ring schrumpft proportional nach innen).
- **Vol-Label zwei Stufen kleiner:** `montserrat_38 → montserrat_34`
  (2px-Raster, 38→36→34).

**Status:** Auf `master` gemergt. Build grün, **hardware-verifiziert** (geflasht, sieht gut aus).

## Geänderte Dateien

- `src/ui.cpp` — `R_OUT`/`R_IN` (Z. 17-18), Vol-Label-Font (Z. 174).
- `src/lv_conf.h` — `LV_FONT_MONTSERRAT_34` aktiviert, `_38` deaktiviert
  (38 war nirgends sonst referenziert → Flash gespart).

## Verifikation

- Firmware-Build: **SUCCESS** (Flash 55.2 %).
- On-Device: User geflasht, Arc + Label sitzen wie gewünscht.

## Hinweise

- Andere Labels (Hints 20/24, Mute/Feedback/Countdown 48) unverändert.
- Mute-Kreuz-Punkte (`s_mute_cross_*_pts`) hängen am Mute-Icon (48), nicht am Vol-Label
  — daher bewusst nicht angepasst.
