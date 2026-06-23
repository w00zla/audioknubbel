# Handoff - Ice Neon Theme (2026-06-21)

## Stand

Branch: `codex/add-ice-neon-theme`

Das neue Theme `Ice Neon` wurde nach visuellem Review-Gate integriert. Die Firmware enthaelt jetzt
6 Themes mit je 15 Background-Stufen:

- `Plasma`
- `Cyberpunk`
- `BIOS CRT`
- `Deep Space`
- `Lava Core`
- `Ice Neon`

## Artwork-Workflow

Der Theme-Workflow-Gate wurde eingehalten:

1. Zuerst Design-Spec geschrieben und freigegeben.
2. Danach nur visuelle Asset-Arbeit.
3. Raw- und Overlay-Kontaktbogen gezeigt.
4. Erst nach expliziter Freigabe der 15 Ice-Neon-Stufen wurde Firmware integriert.

Wichtig: Die 15 Stufen wurden nicht per Script kuenstlerisch erzeugt, reveal-maskiert,
aufgefuellt, normalisiert oder umsortiert. Das Script fuer Ice Neon macht nur technische Schritte:
Sheet-Zuschnitt, 240x240-Export und Kontaktboegen.

## Neue Assets

Quelle:

- `docs/mockups/theme-sources/ice-neon-sheet.png`

Einzelstufen:

- `docs/mockups/ice-neon-15-00..14.png`

Review:

- `docs/mockups/ice-neon-15-raw-contact-sheet.png`
- `docs/mockups/ice-neon-15-overlay-contact-sheet.png`

Design/Plan:

- `docs/superpowers/specs/2026-06-21-ice-neon-theme-design.md`
- `docs/superpowers/plans/2026-06-21-ice-neon-theme-assets.md`

## Technische Integration

- `src/ui_backgrounds.h`
  - neue `LV_IMG_DECLARE`s fuer `ui_bg_ice_00..14`
- `src/ui_theme.cpp`
  - neues `ICE_NEON_BG` Array
  - neuer `UI_THEMES` Eintrag `Ice Neon`
- `src/ui_backgrounds.cpp`
  - neu generiert, enthaelt jetzt Ice-Neon-Bilddaten
- `tools/convert_ui_background_pngs.ps1`
  - neue Asset-Range `ui_bg_ice` / `ice-neon`
- `tools/make_ice_neon_review_assets.ps1`
  - erzeugt die 240x240 Einzelstufen und raw/overlay Kontaktboegen aus dem Ice-Neon-Sheet

Nicht angefasst:

- `src/display.cpp`
- Display-/Backlight-/LED-Ring-GPIOs 1/2/40/46/48

## Verifikation

Build auf `codex/add-ice-neon-theme`:

```powershell
C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3
```

Ergebnis:

```text
SUCCESS
RAM:   34.5% (112908 / 327680)
Flash: 67.2% (11180792 / 16646144)
```

`firmware.bin` wurde erzeugt:

```text
.pio/build/crowpanel-s3/firmware.bin
```

## Flash-Hinweis

Nicht geflasht. Nach Projektregel erst nach explizitem `go` uploaden, wenn das Board im
Download-Modus ist:

```powershell
C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3 -t upload
```

## Offene Hinweise

- Nach Merge auf `master` sollte der PlatformIO-Build dort erneut laufen.
- Die PowerShell-Ausgaben zu `Set-PSReadLineOption` kommen aus dem User-Profil und sind keine
  Build- oder Git-Fehler.
- `git status` meldet Warnungen wegen fehlendem Zugriff auf
  `C:\Users\w00zla\.config\git\ignore`; das blockiert lokale Git-Operationen nicht.
