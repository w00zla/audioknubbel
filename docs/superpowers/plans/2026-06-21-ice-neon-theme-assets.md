# Ice Neon Theme Assets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a reviewable `Ice Neon` 15-stage visual sequence with raw and rough firmware-overlay contact sheets.

**Architecture:** This plan stops at visual review assets. The source artwork is generated or curated as bitmap stages, then scripts are used only for technical crop/export and contact-sheet assembly. Firmware conversion and registry integration are separate work after explicit user approval.

**Tech Stack:** Built-in image generation for bitmap concept/source artwork, PowerShell plus `System.Drawing` for technical crop/contact-sheet generation, PNG review artifacts under `docs/mockups/`.

## Global Constraints

- No firmware integration before user approval of the 15-stage visual sequence.
- Do not modify `src/ui_backgrounds.cpp`, `src/ui_backgrounds.h`, `src/ui_theme.cpp`, or any display GPIO code in this plan.
- Do not run PlatformIO build in this plan.
- Scripts may only perform technical steps: crop/export, rough firmware-overlay contact sheets, hashes/archive, or later RGB565 conversion after approval.
- Scripts must not artistically generate, fill, reveal-mask, reorder, normalize, or rescue the 15 stages.
- The visual rule is: more volume means more visible background energy.
- Stages are ordered `00..14`.

---

### Task 1: Create Ice Neon Source Sheet

**Files:**
- Create: `docs/mockups/theme-sources/ice-neon-sheet.png`

**Interfaces:**
- Consumes: `docs/superpowers/specs/2026-06-21-ice-neon-theme-design.md`
- Produces: a 3x5 source sheet containing 15 visually authored Ice Neon stages in row-major order.

- [ ] **Step 1: Generate or curate the source sheet**

Use this prompt as the source-art target:

```text
Use case: stylized-concept
Asset type: source artwork sheet for a 240x240 round ESP32 volume knob display theme
Primary request: Create one clean 3-by-5 sheet containing 15 distinct square bitmap background tiles for a theme called Ice Neon. The tiles must be ordered left-to-right, top-to-bottom from stage 00 to stage 14, with a clear progression from low volume to full volume. No text, labels, numbers, UI controls, icons, symbols, or watermarks.
Scene/backdrop: abstract frozen crystal core with neon refractions, designed for a round display crop.
Subject: dark glassy ice at stage 00, faint cyan cracks in early stages, luminous crystalline rings in middle stages, and electric cyan-magenta aurora energy inside a bright ice core in late stages.
Style/medium: polished futuristic game UI background art, sharp crystalline texture, luminous but not cluttered, readable at 240x240.
Composition/framing: each tile is square and self-contained, centered circular motif for round display crop, no cropped circle edges, no neighboring tile bleed.
Lighting/mood: cold, clean, increasingly energized stage by stage.
Color palette: black, deep navy, cyan, icy blue, white, restrained magenta highlights.
Constraints: the center of each tile stays calm enough for arc and percent text; outer areas may carry stronger crystal shards and glow; stage 14 is full bright energized ice but still readable.
Avoid: readable glyphs, letters, numbers, UI overlays, large borders, random jumps between stages.
```

Save the selected output to:

```powershell
Copy-Item -LiteralPath "<generated-image-path>" -Destination "docs/mockups/theme-sources/ice-neon-sheet.png"
```

Expected: `docs/mockups/theme-sources/ice-neon-sheet.png` exists and visibly contains 15 staged tiles.

- [ ] **Step 2: Inspect the source sheet manually**

Open or view `docs/mockups/theme-sources/ice-neon-sheet.png`.

Expected:
- 15 tiles are present.
- Progression increases from dark ice to bright neon ice.
- No text, labels, UI controls, or obvious watermark.
- No tile depends on script-side reveal or reordering.

- [ ] **Step 3: Commit the source sheet**

```powershell
git add docs/mockups/theme-sources/ice-neon-sheet.png
git commit -m "art: add ice neon source sheet" -m "Co-Authored-By: Codex Opus 4.8 <noreply@anthropic.com>"
```

Expected: commit succeeds.

---

### Task 2: Add Technical Review-Asset Script

**Files:**
- Create: `tools/make_ice_neon_review_assets.ps1`
- Create: `docs/mockups/ice-neon-15-00.png` through `docs/mockups/ice-neon-15-14.png`
- Create: `docs/mockups/ice-neon-15-raw-contact-sheet.png`
- Create: `docs/mockups/ice-neon-15-overlay-contact-sheet.png`

**Interfaces:**
- Consumes: `docs/mockups/theme-sources/ice-neon-sheet.png`
- Produces: 15 exported review stages and two contact sheets.

- [ ] **Step 1: Create the script**

Create `tools/make_ice_neon_review_assets.ps1` with this content:

```powershell
param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputDir = "docs/mockups"
)

Add-Type -AssemblyName System.Drawing

$out = Join-Path $Root $OutputDir
$sourcePath = Join-Path $out "theme-sources/ice-neon-sheet.png"
New-Item -ItemType Directory -Force -Path $out | Out-Null

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source sheet: $sourcePath"
}

$tile = 240
$cols = 5
$rows = 3

function Save-Stage($src, [int]$stage, [string]$path) {
    $cellW = [Math]::Floor($src.Width / $cols)
    $cellH = [Math]::Floor($src.Height / $rows)
    $col = $stage % $cols
    $row = [Math]::Floor($stage / $cols)
    $srcRect = [System.Drawing.Rectangle]::new($col * $cellW, $row * $cellH, $cellW, $cellH)

    $final = [System.Drawing.Bitmap]::new($tile, $tile, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $g = [System.Drawing.Graphics]::FromImage($final)
        try {
            $g.Clear([System.Drawing.Color]::Black)
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $g.DrawImage($src, [System.Drawing.Rectangle]::new(0, 0, $tile, $tile), $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
        }
        finally { $g.Dispose() }
        $final.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $final.Dispose() }
}

function Draw-Ring($g, [int]$x, [int]$y, [int]$size) {
    $scale = $size / 240.0
    $penOuter = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(128, 210, 230, 255), [float](1.2 * $scale))
    $penTrack = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(135, 18, 28, 44), [float](3.0 * $scale))
    $penArc = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(230, 185, 245, 255), [float](3.8 * $scale))
    $brushCore = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(210, 0, 0, 0))
    $brushPad = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(160, 0, 0, 0))
    try {
        foreach ($p in @($penTrack, $penArc)) {
            $p.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
            $p.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        }
        $g.DrawEllipse($penOuter, $x + (1 * $scale), $y + (1 * $scale), 238 * $scale, 238 * $scale)
        $g.DrawArc($penTrack, $x + (61 * $scale), $y + (61 * $scale), 118 * $scale, 118 * $scale, 135, 270)
        $g.DrawArc($penArc, $x + (61 * $scale), $y + (61 * $scale), 118 * $scale, 118 * $scale, 135, 190)
        $g.FillEllipse($brushCore, $x + (81 * $scale), $y + (81 * $scale), 78 * $scale, 78 * $scale)
        $g.FillEllipse($brushPad, $x + (101 * $scale), $y + (10 * $scale), 38 * $scale, 38 * $scale)
        $g.FillEllipse($brushPad, $x + (10 * $scale), $y + (101 * $scale), 38 * $scale, 38 * $scale)
        $g.FillEllipse($brushPad, $x + (192 * $scale), $y + (101 * $scale), 38 * $scale, 38 * $scale)
        $g.FillEllipse($brushPad, $x + (106 * $scale), $y + (202 * $scale), 28 * $scale, 28 * $scale)
    }
    finally {
        $penOuter.Dispose()
        $penTrack.Dispose()
        $penArc.Dispose()
        $brushCore.Dispose()
        $brushPad.Dispose()
    }
}

function Make-ContactSheet([string]$path, [bool]$overlay) {
    $thumb = 120
    $pad = 12
    $labelH = 32
    $width = (5 * $thumb) + (4 * $pad)
    $height = $labelH + (3 * $thumb) + (2 * $pad) + 20
    $sheet = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $g = [System.Drawing.Graphics]::FromImage($sheet)
        try {
            $g.Clear([System.Drawing.Color]::FromArgb(5, 5, 7))
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $font = [System.Drawing.Font]::new("Segoe UI", 13, [System.Drawing.FontStyle]::Bold)
            $small = [System.Drawing.Font]::new("Segoe UI", 9, [System.Drawing.FontStyle]::Regular)
            $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(232, 242, 255))
            $muted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(160, 176, 190))
            try {
                $suffix = if ($overlay) { " + firmware overlay" } else { "" }
                $g.DrawString(("Ice Neon raw stages{0}" -f $suffix), $font, $brush, 0, 5)
                for ($i = 0; $i -lt 15; $i++) {
                    $col = $i % 5
                    $row = [Math]::Floor($i / 5)
                    $x = $col * ($thumb + $pad)
                    $y = $labelH + ($row * ($thumb + $pad))
                    $imgPath = Join-Path $out ("ice-neon-15-{0:d2}.png" -f $i)
                    $img = [System.Drawing.Bitmap]::FromFile($imgPath)
                    try { $g.DrawImage($img, $x, $y, $thumb, $thumb) }
                    finally { $img.Dispose() }
                    if ($overlay) { Draw-Ring $g $x $y $thumb }
                    $g.DrawString(("{0:d2}" -f $i), $small, $muted, $x + 4, $y + 3)
                }
            }
            finally {
                $font.Dispose()
                $small.Dispose()
                $brush.Dispose()
                $muted.Dispose()
            }
        }
        finally { $g.Dispose() }
        $sheet.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $sheet.Dispose() }
}

$src = [System.Drawing.Bitmap]::FromFile($sourcePath)
try {
    for ($i = 0; $i -lt 15; $i++) {
        $target = Join-Path $out ("ice-neon-15-{0:d2}.png" -f $i)
        Save-Stage $src $i $target
    }
}
finally { $src.Dispose() }

Make-ContactSheet (Join-Path $out "ice-neon-15-raw-contact-sheet.png") $false
Make-ContactSheet (Join-Path $out "ice-neon-15-overlay-contact-sheet.png") $true

Write-Host "Wrote Ice Neon review assets to $out"
```

Expected: script exists and contains no artistic generation logic.

- [ ] **Step 2: Run the script**

```powershell
.\tools\make_ice_neon_review_assets.ps1
```

Expected output includes:

```text
Wrote Ice Neon review assets to
```

- [ ] **Step 3: Verify exported files exist**

```powershell
Get-ChildItem -LiteralPath docs/mockups -Filter 'ice-neon-15-*.png' | Select-Object -ExpandProperty Name
```

Expected: names include `ice-neon-15-00.png` through `ice-neon-15-14.png`,
`ice-neon-15-raw-contact-sheet.png`, and `ice-neon-15-overlay-contact-sheet.png`.

- [ ] **Step 4: Commit review assets**

```powershell
git add tools/make_ice_neon_review_assets.ps1 docs/mockups/ice-neon-15-*.png
git commit -m "art: add ice neon review assets" -m "Co-Authored-By: Codex Opus 4.8 <noreply@anthropic.com>"
```

Expected: commit succeeds.

---

### Task 3: User Review Gate

**Files:**
- Read: `docs/mockups/ice-neon-15-raw-contact-sheet.png`
- Read: `docs/mockups/ice-neon-15-overlay-contact-sheet.png`

**Interfaces:**
- Consumes: review contact sheets from Task 2.
- Produces: explicit user approval or requested visual revisions.

- [ ] **Step 1: Show the contact sheets**

Present both files to the user:

```markdown
![Ice Neon raw contact sheet](D:/ProjectsLocal/audioknubbel/docs/mockups/ice-neon-15-raw-contact-sheet.png)
![Ice Neon firmware overlay contact sheet](D:/ProjectsLocal/audioknubbel/docs/mockups/ice-neon-15-overlay-contact-sheet.png)
```

- [ ] **Step 2: Ask the gate question**

Ask exactly:

```text
Sind diese 15 Ice-Neon-Stufen so freigegeben?
```

Expected: user says yes, or provides concrete visual changes.

- [ ] **Step 3: Stop before firmware work**

If the user approves, write the next plan for firmware integration. If the user requests changes,
return to Task 1 and create a revised source sheet. Do not modify firmware files in this plan.
