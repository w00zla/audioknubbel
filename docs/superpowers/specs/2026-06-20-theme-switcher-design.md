# Theme-Switcher + Repartition — Design/Spec

**Datum:** 2026-06-20 · **Branch:** `feature/theme-switcher` · **Scope:** Infrastruktur, N-Theme-fähig

## Ziel

Mehr Themes ermöglichen (Ziel 3–4) ohne am 3,19-MB-App-Slot anzustoßen, plus ein
dedizierter Theme-Switcher-View. **Diese Aufgabe liefert nur die Infrastruktur** —
läuft mit den 2 bestehenden Themes; neue Theme-Grafiken und feinere Stufen sind
eigene Folgeaufgaben.

## Ausgangslage (gemessen)

- 16 MB Flash, aber Partitionstabelle mappt nur 8 MB; obere 8 MB unallokiert.
- App-Slot `app0` = 3,19 MB, Firmware = 2,75 MB → nur ~447 KB frei (~3–4 Vollbild-BGs).
- 18 Hintergründe (240×240 RGB565, je 112,5 KB) = ~2,02 MB → dominieren das Binary.
- Zweiter OTA-Slot `app1` (3,19 MB) + `spiffs` (1,5 MB) sind **ungenutzt** (Flash per USB-Download, kein FS).

## Teil 1 — Repartition

Neue eingecheckte `partitions.csv` + `board_build.partitions = partitions.csv`:

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| `nvs` | data | nvs | `0x9000` | `0x5000` |
| `factory` | app | factory | `0x10000` | `0xFE0000` (~15,9 MB) |
| `coredump` | data | coredump | `0xFF0000` | `0x10000` |

- **`nvs` Offset/Size unverändert** → normaler `pio upload` (kein Full-Erase) erhält
  Helligkeit/Standby/Theme. Ein expliziter `erase_flash` setzt auf Defaults (sinnvoll).
- Kein `otadata` → Bootloader bootet `factory`. Kein OTA-Verlust (USB-Download bleibt).

## Teil 2 — Theme-Registry (Generalisierung)

`ui_theme.h` wird vom Enum zur Daten-Tabelle, `ui_theme.cpp` neu:

```c
struct UiRgb { uint8_t r, g, b; };
struct UiThemeDef {
    const char*                name;          // Switcher-Label
    const lv_img_dsc_t* const* backgrounds;   // [band_count]
    uint8_t                    band_count;    // aktuell 9
    UiRgb                      zone_low, zone_mid, zone_high;  // Tick-Ring-Zonen
    UiRgb                      dim;           // erloschene Ticks
    bool                       show_media_hints;
};
extern const UiThemeDef UI_THEMES[];
extern const int        UI_THEME_COUNT;
int ui_theme_wrap(int index);   // mod-wrap für Switcher-Scroll
```

Die 2 bestehenden Themes (Plasma, Cyberpunk) werden 1:1 als Tabelleneinträge
abgebildet (Farben/Flags aus der bisherigen `if (CYBERPUNK)`-Logik). In `ui.cpp`
lesen `background_src`, `zone_color`, `apply_ticks` (dim), `apply_theme_visibility`
künftig aus `UI_THEMES[s_theme]`; `s_theme` wird ein `int`-Index. Neues Theme =
ein Tabelleneintrag + `LV_IMG_DECLARE`s, kein Logik-Eingriff.

## Teil 3 — Switcher-View, Bedienung, Persistenz

**View (Variante B):** Vollbild-Overlay über der Haupt-UI. Live-Hintergrund = Vorschau.
Zentrierter Theme-Name (`montserrat_24`, Pill-Hintergrund), kleines „THEME"-Eyebrow
(`montserrat_14`) darüber, Positions-Punkte unten (dynamisch UI_THEME_COUNT, aktiver
breiter/heller). HUD (Ticks/Vol/Hints/Conn-Dot) wird im Switcher ausgeblendet.

**State-Machine (`main.cpp`):**
- **Normal:** Dreh=Volume, Druck=Mute, **Long-Press=Switcher öffnen**, Gesten=Media.
- **Switcher offen:** beim Öffnen aktuellen Index als *Original* merken → Dreh=Index ±
  + Theme **live anwenden** → **Long-Tap=übernehmen** (NVS speichern, `THEME:<name>` senden,
  schließen) → **Single-Tap=abbrechen** (Original wiederherstellen, schließen). Encoder-Druck
  ohne Funktion im Switcher. Media-Gesten + Volume-getriebener BG-Wechsel sind unterdrückt
  (`apply_background()` early-return bei offenem Switcher).
- **Encoder-Entkopplung:** Der Knob liefert mehrere Ticks pro mechanischer Raststellung;
  im Switcher zählt nur die Drehrichtung, ein Cooldown (`SW_SCROLL_COOLDOWN_MS`, 150 ms)
  fasst den Tick-Burst einer Raststellung zu genau einem Schritt zusammen.

**Persistenz:** neues `theme_cfg.h/.cpp` (Muster `standby_cfg`, NVS-Namespace `audioknubbel`,
Key `theme`, Default 0). Boot lädt Index → `ui_theme_set(idx)` (gegen `ui_theme_count()`
geclampt).

## API (`ui.h`)

`ui_toggle_theme()` entfällt. Neu:
```c
void ui_theme_set(int index);   int ui_theme_get();
const char* ui_theme_name(int index);   int ui_theme_count();
void ui_switcher_open();   void ui_switcher_scroll(int delta);
int  ui_switcher_selected();   void ui_switcher_close();   bool ui_switcher_is_open();
```

## Geänderte/neue Dateien

`partitions.csv` (neu), `platformio.ini`, `theme_cfg.h/.cpp` (neu), `ui_theme.h`
(rewrite) + `ui_theme.cpp` (neu), `ui.h` (API), `ui.cpp` (Registry-Refactor +
Switcher-Overlay), `main.cpp` (Mode-Logik + Boot-Load).

## Verifikation

- Host-Build via `pio run -e crowpanel-s3` (kein nativer Test-Pfad, vgl. CLAUDE.md).
- On-device (nach „go"): Long-Press öffnet Switcher; Dreh wechselt BG+Ring live;
  Druck übernimmt + bleibt nach Reboot erhalten; Tap/Long-Press bricht zum Original ab;
  Helligkeit/Standby überleben den Repartition-Flash.
