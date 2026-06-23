# Handoff - BIOS CRT, Deep Space, Lava Core Themes (2026-06-21)

## Stand

Branch: `master`

Commit mit Firmware-Integration:

```text
8e2c54a Add BIOS, space, and lava themes
```

Die Firmware enthaelt jetzt 5 Themes mit je 15 Background-Stufen:

- `Plasma`
- `Cyberpunk`
- `BIOS CRT`
- `Deep Space`
- `Lava Core`

Der Feature-Branch `codex/add-bios-space-lava-themes` wurde per Fast-forward nach
`master` gemerged und danach lokal geloescht.

## Artwork-Workflow

Der zweite Versuch lief bewusst nach dem Theme-Workflow-Gate:

1. Zuerst nur visuelle Asset-Arbeit.
2. Kein `src/ui_backgrounds.cpp`, keine Firmware-Integration und kein PlatformIO-Build vor
   visueller Freigabe.
3. Kontaktbogen raw + Kontaktbogen mit grobem Firmware-Overlay.
4. Erst danach technische Konvertierung und Registry-Einbindung.

Wichtig: Die 15 Stufen wurden nicht per Script kuenstlerisch erzeugt, reveal-maskiert,
aufgefuellt oder umsortiert. Scripts wurden nur fuer technische Schritte genutzt:
Zuschnitt aus den kuratierten Sheets, Kontaktbogen/Overlay und RGB565/LVGL-Konvertierung.

## Neue Assets

Quellen:

- `docs/mockups/theme-sources/bios-crt-sheet.png`
- `docs/mockups/theme-sources/deep-space-sheet.png`
- `docs/mockups/theme-sources/lava-core-sheet.png`

Einzelstufen:

- `docs/mockups/bios-crt-15-00..14.png`
- `docs/mockups/deep-space-15-00..14.png`
- `docs/mockups/lava-core-15-00..14.png`

Review:

- `docs/mockups/new-themes-15-raw-contact-sheet.png`
- `docs/mockups/new-themes-15-overlay-contact-sheet.png`

Deep Space wurde nach User-Feedback neu gemacht: symmetrischerer Nebelkranz, und Stufe
14 ist nun klar gefuellt. BIOS CRT und Lava Core blieben nach Freigabe unveraendert.

## Technische Integration

- `src/ui_backgrounds.h`
  - neue `LV_IMG_DECLARE`s fuer `ui_bg_bios_00..14`,
    `ui_bg_space_00..14`, `ui_bg_lava_00..14`
- `src/ui_theme.cpp`
  - neue Theme-Arrays und `UI_THEMES`-Eintraege fuer `BIOS CRT`, `Deep Space`,
    `Lava Core`
- `src/ui_backgrounds.cpp`
  - neu generiert, enthaelt jetzt 75 LVGL-Image-Deskriptoren
- `tools/convert_ui_background_pngs.ps1`
  - auf datengetriebene Asset-Ranges umgestellt
  - Plasma bleibt mit Downtone + Icon-Pad-Masken
  - Cyberpunk und die drei neuen Themes bleiben ohne Downtone und ohne Icon-Pad-Masken
- `tools/make_new_theme_review_assets.ps1`
  - erzeugt die 240x240 Einzelstufen aus den 3x5-Sheets
  - erzeugt raw/overlay Kontaktboegen
- `platformio.ini`
  - `board_upload.maximum_size = 16646144`, passend zum `0xFE0000` Factory-Slot aus
    `partitions.csv`

## Verifikation

Build auf `master`:

```powershell
C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3
```

Ergebnis:

```text
SUCCESS
RAM:   34.5% (112908 / 327680)
Flash: 56.8% (9452520 / 16646144)
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

- Die PowerShell-Ausgaben zu `Set-PSReadLineOption` kommen aus dem User-Profil und sind
  keine Build- oder Git-Fehler.
- `git status` meldete wiederholt Warnungen wegen fehlendem Zugriff auf
  `C:\Users\w00zla\.config\git\ignore`; das hat die lokalen Git-Operationen nicht blockiert.
- `display.cpp` und die kritischen GPIOs 1/2/40/46 wurden nicht angefasst.
