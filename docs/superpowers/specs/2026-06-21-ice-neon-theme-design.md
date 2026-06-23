# Ice Neon Theme Design

**Date:** 2026-06-21  
**Branch:** `codex/add-ice-neon-theme`  
**Scope:** visual asset design for one additional 15-stage theme

## Goal

Add a new theme direction named `Ice Neon` for the Crow Knob display. The theme should feel cold,
sharp, and electric, distinct from Plasma, Cyberpunk, BIOS CRT, Deep Space, and Lava Core.

The first phase is visual asset work only. Firmware integration is explicitly out of scope until
the 15-stage sequence is visually approved.

## Visual Direction

`Ice Neon` uses a dark glass-and-ice base with cyan, icy blue, white, and restrained magenta
highlights. The subject is a centered crystalline energy core suitable for a 240x240 round display.

The progression must be authored into the bitmap stages themselves:

- Stage 00: nearly black frozen glass, only faint cold structure.
- Early stages: cyan cracks and subtle crystalline rings begin to appear.
- Middle stages: the ice core becomes clearer, brighter, and more geometric.
- Late stages: cyan-magenta aurora energy fills more of the core and surrounding ice.
- Stage 14: full bright energized ice core, intense but still readable under the firmware overlay.

The rule is: more volume means more visible background energy.

## Asset Requirements

- 15 curated bitmap stages, ordered `00..14`.
- Each stage is a 240x240 PNG after technical crop/export.
- Raw source may be a clean 3x5 sheet or individual staged images.
- No text, numbers, symbols, UI controls, icons, watermarks, or readable glyphs.
- No cropped circular motifs, neighboring tile bleed, or stage-to-stage random jumps.
- The center must remain quiet enough for the existing arc, percent text, and mute label.
- Outer areas may carry stronger crystal shards and glow, especially in later stages.

## Review Gate

Before any firmware integration:

1. Produce the raw 15-stage contact sheet.
2. Produce a second contact sheet with a rough firmware overlay.
3. Ask explicitly whether the 15 stages are approved.

No `src/ui_backgrounds.cpp`, `src/ui_backgrounds.h`, `src/ui_theme.cpp`, PlatformIO build, or flash
work happens before that approval.

## Technical Pipeline After Approval

After visual approval only:

- Use scripts only for technical steps: crop/export, optional central/icon-pad darkening,
  contact sheets, hashes/archive, and RGB565/LVGL conversion.
- Add `LV_IMG_DECLARE` entries for the new `Ice Neon` assets.
- Add an `Ice Neon` entry to `UI_THEMES`.
- Run the PlatformIO build with
  `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`.
- Do not flash until the user explicitly says `go`.

## Self-Review

- No placeholders remain.
- Scope is limited to one new visual theme.
- The approval gate is explicit.
- The design follows the project rule that scripts must not artistically generate, fill, reorder,
  reveal-mask, normalize, or otherwise rescue the 15 stages.
