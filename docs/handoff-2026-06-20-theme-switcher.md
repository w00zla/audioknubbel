# Handoff — Theme-Switcher + N-Theme-Registry + Repartition (2026-06-20)

## Was umgesetzt wurde

Drei zusammenhängende Dinge, **hardware-verifiziert** und auf `master`:

1. **Single-App-Repartition** — gibt dem App-Slot ~15,9 MB statt 3,19 MB Luft für
   künftige Themes/Stufen.
2. **N-Theme-Registry** — die hartcodierte „2-Theme"-Logik wird datengetrieben; ein
   neues Theme ist ein Tabelleneintrag.
3. **Theme-Switcher-View** — Long-Press öffnet ein Overlay, Drehen wählt mit Live-
   Vorschau, Long-Tap übernimmt (NVS-persistiert), Single-Tap bricht ab.

**Scope-Grenze:** Dies ist die **Infrastruktur**. Es laufen weiterhin die 2 bestehenden
Themes (Plasma, Cyberpunk). Neue Theme-Grafiken und feinere Stufen (15 statt 9) sind
bewusst **eigene Folgeaufgaben** (siehe „Offen/Nächste Schritte").

Spec/Design: `docs/superpowers/specs/2026-06-20-theme-switcher-design.md`.

## Repartition — Detail

Neue eingecheckte `partitions.csv` + `board_build.partitions = partitions.csv`:

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| `nvs` | data | nvs | `0x9000` | `0x5000` |
| `factory` | app | factory | `0x10000` | `0xFE0000` (~15,88 MB) |
| `coredump` | data | coredump | `0xFF0000` | `0x10000` |

- Vorher: Dual-OTA (`app0`/`app1` je 3,19 MB) + 1,5 MB `spiffs`, nur 8 MB von 16 MB gemappt.
  `app1`/`otadata`/`spiffs` waren **ungenutzt** (Flash per USB-Download, kein FS).
- **`nvs` Offset/Size unverändert** → ein normaler `pio upload` (kein Full-Erase) erhält
  Helligkeit/Standby/Theme. Wurde am Gerät bestätigt.
- **Caveat:** Ein expliziter `erase_flash` setzt die Config auf Defaults (sinnvoll).
- **Caveat 2 (kosmetisch):** Der Build meldet „Flash … from 8388608 bytes" — das ist
  PlatformIOs `board_upload.maximum_size` (Board-Default), **nicht** die echte Partition
  (15,88 MB). Bei 2,88 MB Firmware irrelevant; falls künftige Assets die 8-MB-Schwelle
  reißen, in `platformio.ini` `board_upload.maximum_size` hochsetzen.

## Theme-Registry — Architektur

- `src/ui_theme.h` (rewrite): aus dem Enum wird `struct UiThemeDef` (Name, Band-Hintergründe,
  3 Tick-Ring-Zonenfarben, Dim-Farbe, `show_media_hints`-Flag) + Tabelle `UI_THEMES` /
  `UI_THEME_COUNT` + `ui_theme_wrap()`.
- `src/ui_theme.cpp` (neu): die 2 Themes 1:1 als Tabelleneinträge (Farben/Flags aus der
  alten `if (CYBERPUNK)`-Logik) + die Background-Pointer-Arrays.
- `src/ui.cpp`: `background_src`, `zone_color`, `apply_ticks` (dim), `apply_theme_visibility`
  lesen aus `UI_THEMES[s_theme]`; `s_theme` ist jetzt ein `int`-Index.
- **Neues Theme einbinden:** Background-Set als `ui_bg_<x>_NN` einkompilieren +
  `LV_IMG_DECLARE`s in `ui_backgrounds.h` + ein Eintrag in `UI_THEMES`. Kein Logik-Eingriff.

## Switcher-View — Bedienung & Persistenz

- **View (Variante B):** Vollbild-Overlay, zentrierter Name (`montserrat_24`, Pill) + kleines
  „THEME"-Eyebrow (`montserrat_14`) + dynamische Positions-Punkte. HUD (Ticks/Vol/Hints/
  Conn-Dot) ist im Switcher ausgeblendet; der Live-Hintergrund ist die Vorschau.
- **State-Machine (`main.cpp`):**
  - Normal: Dreh=Volume, Druck=Mute, **Long-Press=öffnen**, Gesten=Media.
  - Switcher offen: **Dreh=Vorschau**, **Long-Tap=übernehmen** (NVS + `THEME:<name>` + schließen),
    **Single-Tap=abbrechen** (zurück auf Original). Encoder-Druck ohne Funktion.
    Media-Gesten + Volume-BG-Wechsel unterdrückt (`apply_background()` early-return).
- **Encoder-Entkopplung (wichtig, hardware-erkauft):** Der Knob ist mechanisch sehr sensibel
  und liefert **mehrere Ticks pro Raststellung**. Im Switcher zählt nur die Drehrichtung,
  ein Cooldown `SW_SCROLL_COOLDOWN_MS = 150` (in `main.cpp`) fasst den Tick-Burst einer
  Raststellung zu einem Schritt zusammen. (Volume-Modus bleibt unverändert tick-genau.)
- **Persistenz:** `src/theme_cfg.h/.cpp` (neu, Muster `standby_cfg`, NVS-Namespace `audioknubbel`,
  Key `theme`, Default 0). Boot lädt den Index → `ui_theme_set()` (gegen `ui_theme_count()`
  geclampt). Bestätigt: gewähltes Theme überlebt Reboot.

## Geänderte/neue Dateien

- **Neu:** `partitions.csv`, `src/ui_theme.cpp`, `src/theme_cfg.h`, `src/theme_cfg.cpp`,
  Spec + dieser Handoff.
- **Geändert:** `platformio.ini` (`board_build.partitions`), `src/ui_theme.h` (rewrite),
  `src/ui.h` (API: `ui_theme_set/get/count/name`, `ui_switcher_*`; `ui_toggle_theme` entfällt),
  `src/ui.cpp` (Registry-Refactor + Switcher-Overlay), `src/main.cpp` (Mode-Logik + Boot-Load),
  `.gitignore` (`.superpowers/`).

## Bekanntes Verhalten / bewusste Entscheidungen

- **Kein Standby bei offenem Switcher:** solange das Overlay offen ist, bleibt der Screen an
  (Activity-Timer wird pro Loop resettet). Bewusst so.
- `THEME:<name>\n` geht weiterhin an die Companion (rein informativ, kein App-Handler nötig).

## Offen / Nächste Schritte (eigene Aufgaben)

1. **Feinere Stufen:** Band-Logik in `ui_background_state.h` von hartcodierten 9 auf N
   generalisieren (Ziel 15). `UiThemeDef.band_count` trägt die Anzahl bereits — es ändern
   sich primär Daten + die Edge/Hysterese-Tabellen.
2. **Neue Themes (3.–4.):** Background-Sets generieren (`tools/generate_ui_backgrounds.py`)
   und als Tabelleneinträge einbinden. Der Switcher zeigt sie dann automatisch.
3. Optional: Dreh-Cooldown (150 ms) bei Bedarf nachjustieren.
