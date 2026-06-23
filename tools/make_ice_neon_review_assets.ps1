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
