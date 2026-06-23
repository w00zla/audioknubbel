# Handoff — 15 Background Levels (2026-06-21)

## Stand

Branch: `codex/15-bg-levels`

Die beiden vorhandenen Themes laufen jetzt mit 15 Background-Stufen statt 9. Die
neuen Quellen wurden in der Browser-Preview geprüft und anschließend in die
Firmware-Assets übernommen.

## Geändert

- `docs/mockups/plasma-grow-15-00..14.png`
  - komplette 15er-Serie im bestehenden Plasma-Wolken/Filament-Stil
  - Action-Hint-Bereiche bleiben erhalten
- `docs/mockups/cyberpunk-liquid-15-00..14.png`
  - komplette 15er-Serie als sauberer Liquid-BG ohne Action-Hint-Icons/Löcher
  - langsamer Ramp-up; nur Level 14 ist voll gefüllt
- `docs/mockups/theme-15-live-preview.html`
  - lokale Vergleichsseite mit Slider und Level-Thumbnails
- `src/ui_background_state.h`
  - `UI_BG_00..UI_BG_14`
  - formelbasiertes Mapping 0..100 auf 15 Stufen
  - Hysterese bleibt bei 2 Prozentpunkten
- `src/ui_backgrounds.h`, `src/ui_theme.cpp`
  - beide Themes referenzieren je 15 LVGL-Images
  - Cyberpunk-Tick-Ring farblich abgesetzt: acid-lime -> warm yellow -> orange
- `tools/convert_ui_background_pngs.ps1`
  - konvertiert die `*-15-*` Quellen
  - Plasma maskiert weiterhin die Icon-Pads
  - Cyberpunk maskiert keine Icon-Pads, damit keine künstlichen Löcher entstehen
- `src/ui_backgrounds.cpp`
  - neu generiert; enthält 30 Background-Deskriptoren

## Verifikation

Build:

```powershell
C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3
```

Ergebnis: `SUCCESS`

Speicher:

- RAM: 34.5% (`112908` / `327680`)
- Flash: 50.9% (`4267700` / `8388608`)

## Flash-Hinweis

Nicht geflasht. Nach Projektregel ist der Stand nur ready-to-flash; Upload erst
nach explizitem `go`, wenn das Board im Download-Modus ist.
