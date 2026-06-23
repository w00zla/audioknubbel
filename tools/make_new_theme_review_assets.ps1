param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputDir = "docs/mockups"
)

Add-Type -AssemblyName System.Drawing

$out = Join-Path $Root $OutputDir
$sourceDir = Join-Path $out "theme-sources"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$themes = @(
    @{
        Key = "bios-crt"
        Label = "BIOS CRT"
        Source = "bios-crt-sheet.png"
        X = @(@(3,250), @(253,501), @(503,750), @(752,999), @(1002,1250))
        Y = @(@(172,429), @(468,728), @(767,1027))
        Crop = 240
    },
    @{
        Key = "deep-space"
        Label = "Deep Space"
        Source = "deep-space-sheet.png"
        X = @(@(30,267), @(278,501), @(512,735), @(745,973), @(983,1221))
        Y = @(@(92,314), @(401,664), @(826,1094))
        Crop = 240
    },
    @{
        Key = "lava-core"
        Label = "Lava Core"
        Source = "lava-core-sheet.png"
        X = @(@(7,246), @(259,499), @(511,750), @(762,1003), @(1013,1253))
        Y = @(@(154,428), @(468,743), @(786,1061))
        Crop = 240
    }
)

function Rect-Center([int[]]$band) {
    return ($band[0] + $band[1]) / 2.0
}

function Save-Stage($src, [double]$cx, [double]$cy, [int]$crop, [string]$path) {
    $stage = [System.Drawing.Bitmap]::new($crop, $crop, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $g = [System.Drawing.Graphics]::FromImage($stage)
        try {
            $g.Clear([System.Drawing.Color]::Black)
            $left = [int][Math]::Round($cx - ($crop / 2.0))
            $top = [int][Math]::Round($cy - ($crop / 2.0))
            $srcLeft = [Math]::Max(0, $left)
            $srcTop = [Math]::Max(0, $top)
            $srcRight = [Math]::Min($src.Width, $left + $crop)
            $srcBottom = [Math]::Min($src.Height, $top + $crop)
            $srcW = $srcRight - $srcLeft
            $srcH = $srcBottom - $srcTop
            $dstLeft = $srcLeft - $left
            $dstTop = $srcTop - $top
            if ($srcW -gt 0 -and $srcH -gt 0) {
                $srcRect = [System.Drawing.Rectangle]::new($srcLeft, $srcTop, $srcW, $srcH)
                $dstRect = [System.Drawing.Rectangle]::new($dstLeft, $dstTop, $srcW, $srcH)
                $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
                $g.DrawImage($src, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
            }
        }
        finally { $g.Dispose() }

        $final = [System.Drawing.Bitmap]::new(240, 240, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $g = [System.Drawing.Graphics]::FromImage($final)
            try {
                $g.Clear([System.Drawing.Color]::Black)
                $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $g.DrawImage($stage, 0, 0, 240, 240)
            }
            finally { $g.Dispose() }
            $final.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $final.Dispose() }
    }
    finally { $stage.Dispose() }
}

function Draw-Ring($g, [int]$x, [int]$y, [int]$size) {
    $scale = $size / 240.0
    $penOuter = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(128, 210, 220, 230), [float](1.2 * $scale))
    $penTrack = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(130, 32, 36, 48), [float](3.0 * $scale))
    $penArc = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(225, 235, 245, 255), [float](3.8 * $scale))
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
    $themeH = $labelH + (3 * $thumb) + (2 * $pad) + 20
    $width = (5 * $thumb) + (4 * $pad)
    $height = $themes.Count * $themeH
    $sheet = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $g = [System.Drawing.Graphics]::FromImage($sheet)
        try {
            $g.Clear([System.Drawing.Color]::FromArgb(5, 5, 6))
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $font = [System.Drawing.Font]::new("Segoe UI", 13, [System.Drawing.FontStyle]::Bold)
            $small = [System.Drawing.Font]::new("Segoe UI", 9, [System.Drawing.FontStyle]::Regular)
            $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(232, 236, 240))
            $muted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(160, 168, 176))
            try {
                for ($t = 0; $t -lt $themes.Count; $t++) {
                    $theme = $themes[$t]
                    $baseY = $t * $themeH
                    $g.DrawString(("{0} raw stages {1}" -f $theme.Label, ($(if ($overlay) { "+ firmware overlay" } else { "" }))), $font, $brush, 0, $baseY + 5)
                    for ($i = 0; $i -lt 15; $i++) {
                        $col = $i % 5
                        $row = [Math]::Floor($i / 5)
                        $x = $col * ($thumb + $pad)
                        $y = $baseY + $labelH + ($row * ($thumb + $pad))
                        $imgPath = Join-Path $out ("{0}-15-{1:d2}.png" -f $theme.Key, $i)
                        $img = [System.Drawing.Bitmap]::FromFile($imgPath)
                        try { $g.DrawImage($img, $x, $y, $thumb, $thumb) }
                        finally { $img.Dispose() }
                        if ($overlay) { Draw-Ring $g $x $y $thumb }
                        $g.DrawString(("{0:d2}" -f $i), $small, $muted, $x + 4, $y + 3)
                    }
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

foreach ($theme in $themes) {
    $sourcePath = Join-Path $sourceDir $theme.Source
    $src = [System.Drawing.Bitmap]::FromFile($sourcePath)
    try {
        for ($row = 0; $row -lt 3; $row++) {
            for ($col = 0; $col -lt 5; $col++) {
                $stage = ($row * 5) + $col
                $cx = Rect-Center $theme.X[$col]
                $cy = Rect-Center $theme.Y[$row]
                $target = Join-Path $out ("{0}-15-{1:d2}.png" -f $theme.Key, $stage)
                Save-Stage $src $cx $cy $theme.Crop $target
            }
        }
    }
    finally { $src.Dispose() }
}

Make-ContactSheet (Join-Path $out "new-themes-15-raw-contact-sheet.png") $false
Make-ContactSheet (Join-Path $out "new-themes-15-overlay-contact-sheet.png") $true

Write-Host "Wrote new theme review assets to $out"
