# Handoff — Tick-Ring-UI + Standby

**Branch:** `feature/tickring-vu-ui` · **Datum:** 2026-06-13 · **Status:** auf Device geflasht & verifiziert (User)

## Was gemacht wurde

### 1. Neues UI-Design: Tick-Ring (VU/Studio-Look)
Der alte cyan-Arc wurde durch einen **Tick-Ring** ersetzt — angelehnt an ein
Mischpult-/Peakmeter. Design-Referenz (via fal.ai generiert):
`docs/design/knob-tickring-A.png`.

- **`src/ui.cpp`** komplett umgebaut. 37 `lv_line`-Ticks auf einem 270°-Bogen
  (Start 135°, Gap unten — wie zuvor der Arc). Tick-Geometrie wird per
  `cosf/sinf` aus Radius (R_IN 98 / R_OUT 114) berechnet.
- **Drei gleich große Farbzonen** (`zone_color()`): unteres Drittel grün,
  mittleres amber, oberes rot. Nur Ticks bis zum aktuellen Pegel leuchten,
  der Rest ist gedimmt (`apply_ticks()`).
- Mittige Volume-Zahl, Mute (dimmt alle Ticks + „MUTE"-Label) und der rote
  Disconnect-Punkt in der unteren Gap bleiben erhalten.
- **Öffentliche API unverändert:** `ui_init / ui_set_volume / ui_set_mute /
  ui_set_connected`.
- **`src/lv_conf.h`:** `LV_USE_LINE` von 0 → 1 aktiviert (für `lv_line`).

### 2. Standby-Modus
- **`src/main.cpp`:** Nach `STANDBY_TIMEOUT_MS` (= **15000**) ohne Aktivität
  geht das Backlight komplett aus via `displayBacklightSet(false)`.
- **Aktivität** = Encoder-Dreh/-Druck **oder** App-Push von Volume/Mute
  (`protocolApplyVolume/Mute` rufen `wakeUp()`).
- **Aufwachen:** Erste Eingabe im Standby wird *konsumiert* (nur Display an,
  kein Volume-Sprung / Mute-Toggle).
- `display.cpp` und die GPIOs (1/2/40/46) wurden **nicht** angefasst — die
  vorhandene `displayBacklightSet()`-API reicht.

## Verifizierung
- Firmware gebaut (`pio run -e crowpanel-s3`, Flash 21,5 %) und vom User
  geflasht — Tick-Ring + 15s-Standby laufen auf der Hardware.
- Companion läuft E2E (verbindet auf **COM6**, VID_303A).

## Nebenbefund (kein Code-Change)
- Kurzzeitig „roter Punkt kommt nach Event wieder" beobachtet — ließ sich
  **nicht reproduzieren** und verschwand nach Companion-Neustart. Vermutlich
  zickige/doppelte Port-Belegung beim ersten Start, kein echter Bug.
  Port-Discovery ist sauber (nur ein Port mit VID_303A). Temporär eingebautes
  Diagnose-Logging wurde wieder entfernt.

## Offene Tuning-Optionen (falls gewünscht)
- Tick-Anzahl (37) / -Breite (3 px) / Radien für dichteren/feineren Ring.
- Dezenter Glow-Halo hinter dem aktiven Segment (zweiter, breiter Arc).
- Standby: sanftes PWM-Abdimmen statt hartem Aus.

## Nächster Schritt
Branch `feature/tickring-vu-ui` → `master` mergen (wie bisherige Feature-Branches).
