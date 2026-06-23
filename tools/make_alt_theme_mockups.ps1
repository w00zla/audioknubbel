param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputDir = "docs/mockups"
)

Add-Type -AssemblyName System.Drawing

$out = Join-Path $Root $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

$scale = 3
$canvas = 240 * $scale
$center = 120 * $scale
$arcStart = 135.0
$arcSweep = 270.0
$dockAngles = @(0, 90, 180, 270)

function C([int]$a, [int]$r, [int]$g, [int]$b) {
    [System.Drawing.Color]::FromArgb($a, $r, $g, $b)
}

function B([System.Drawing.Color]$color) {
    [System.Drawing.SolidBrush]::new($color)
}

function PR([System.Drawing.Color]$color, [float]$width) {
    $p = [System.Drawing.Pen]::new($color, $width)
    $p.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $p.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $p.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $p
}

function P([float]$r, [float]$deg) {
    $rad = $deg * [Math]::PI / 180.0
    [System.Drawing.PointF]::new(
        [float]($center + ($r * $scale) * [Math]::Cos($rad)),
        [float]($center + ($r * $scale) * [Math]::Sin($rad))
    )
}

function AD([float]$a, [float]$b) {
    [Math]::Abs((($a - $b + 540.0) % 360.0) - 180.0)
}

function Reserved([float]$deg, [float]$width = 19.0) {
    foreach ($axis in $dockAngles) {
        if ((AD $deg $axis) -lt $width) { return $true }
    }
    return $false
}

function InDonut([float]$x, [float]$y, [float]$rInner, [float]$rOuter) {
    $dx = ($x / $scale) - 120.0
    $dy = ($y / $scale) - 120.0
    $r = [Math]::Sqrt($dx * $dx + $dy * $dy)
    return ($r -ge $rInner -and $r -le $rOuter)
}

function CircleFill($g, [float]$x, [float]$y, [float]$r, $brush) {
    $g.FillEllipse($brush, $x - $r, $y - $r, $r * 2, $r * 2)
}

function CircleStroke($g, [float]$x, [float]$y, [float]$r, $pen) {
    $g.DrawEllipse($pen, $x - $r, $y - $r, $r * 2, $r * 2)
}

function ArcLine($g, [float]$r, [float]$from, [float]$to, $pen, [float]$step = 2.0, [float]$reserve = 16.0) {
    if ($to -le $from) { return }
    $pts = [System.Collections.Generic.List[System.Drawing.PointF]]::new()
    for ($d = $from; $d -le $to; $d += $step) {
        if (-not (Reserved $d $reserve)) { $pts.Add((P $r $d)) }
        elseif ($pts.Count -gt 1) {
            $g.DrawLines($pen, $pts.ToArray())
            $pts.Clear()
        }
    }
    if (-not (Reserved $to $reserve)) { $pts.Add((P $r $to)) }
    if ($pts.Count -gt 1) { $g.DrawLines($pen, $pts.ToArray()) }
}

function LinePolar($g, [float]$r1, [float]$r2, [float]$deg, $pen) {
    $g.DrawLine($pen, (P $r1 $deg), (P $r2 $deg))
}

function Cyber([float]$t, [string]$mode = "mix") {
    $t = [Math]::Max(0.0, [Math]::Min(1.0, $t))
    if ($mode -eq "acid") {
        if ($t -lt .68) { return [System.Drawing.Color]::FromArgb(0, [int](205 + 45 * $t), 255) }
        return [System.Drawing.Color]::FromArgb([int](120 + 135 * (($t - .68) / .32)), 255, 0)
    }
    if ($mode -eq "pink") {
        if ($t -lt .55) { return [System.Drawing.Color]::FromArgb([int](20 + 180 * $t), 60, 255) }
        return [System.Drawing.Color]::FromArgb(255, [int](60 + 105 * (($t - .55) / .45)), [int](255 - 165 * (($t - .55) / .45)))
    }
    if ($t -lt .45) { return [System.Drawing.Color]::FromArgb(0, [int](210 + 45 * $t), 255) }
    if ($t -lt .76) { return [System.Drawing.Color]::FromArgb([int](70 + 185 * (($t - .45) / .31)), 70, 255) }
    return [System.Drawing.Color]::FromArgb(255, [int](90 + 140 * (($t - .76) / .24)), 0)
}

function TextCentered($g, [string]$txt, [float]$y, [float]$h, $font, $brush) {
    $sf = [System.Drawing.StringFormat]::new()
    try {
        $sf.Alignment = [System.Drawing.StringAlignment]::Center
        $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
        $g.DrawString($txt, $font, $brush, [System.Drawing.RectangleF]::new(0, $y * $scale, $canvas, $h * $scale), $sf)
    }
    finally { $sf.Dispose() }
}

function Background($g) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    try {
        $path.AddEllipse(1 * $scale, 1 * $scale, 238 * $scale, 238 * $scale)
        $bg = [System.Drawing.Drawing2D.PathGradientBrush]::new($path)
        try {
            $bg.CenterColor = C 255 6 7 14
            $bg.SurroundColors = [System.Drawing.Color[]]@(C 255 0 0 2)
            $g.FillPath($bg, $path)
        }
        finally { $bg.Dispose() }
    }
    finally { $path.Dispose() }

    $outer = [System.Drawing.Pen]::new((C 150 54 58 68), 1.0 * $scale)
    $safe = [System.Drawing.Pen]::new((C 72 65 68 82), 0.7 * $scale)
    try {
        CircleStroke $g $center $center (118 * $scale) $outer
        CircleStroke $g $center $center (65 * $scale) $safe
        CircleStroke $g $center $center (40 * $scale) $safe
    }
    finally { $outer.Dispose(); $safe.Dispose() }
}

function Docks($g) {
    $dock = B (C 250 1 2 8)
    $cyan = [System.Drawing.Pen]::new((C 130 0 245 255), 0.9 * $scale)
    try {
        foreach ($r in @(
            [System.Drawing.RectangleF]::new(101 * $scale, 9 * $scale, 38 * $scale, 38 * $scale),
            [System.Drawing.RectangleF]::new(9 * $scale, 101 * $scale, 38 * $scale, 38 * $scale),
            [System.Drawing.RectangleF]::new(193 * $scale, 101 * $scale, 38 * $scale, 38 * $scale)
        )) {
            $g.FillEllipse($dock, $r)
            $g.DrawEllipse($cyan, $r)
        }

        $status = [System.Drawing.RectangleF]::new(106 * $scale, 202 * $scale, 28 * $scale, 28 * $scale)
        $g.FillEllipse($dock, $status)
        $g.DrawEllipse($cyan, $status)
    }
    finally { $dock.Dispose(); $cyan.Dispose() }
}

function VolumeArc($g, [int]$value, [string]$mode) {
    if ($mode -eq "segmented") {
        $segments = 18
        $lit = [Math]::Round($segments * $value / 100.0)
        $track = PR (C 72 38 40 50) (3.4 * $scale)
        try { ArcLine $g 58 $arcStart ($arcStart + $arcSweep) $track 1.5 -1 } finally { $track.Dispose() }

        $end = $arcStart + $arcSweep * ($value / 100.0)
        $progressGlow = PR (C 76 110 70 255) (10.0 * $scale)
        $progress = PR (C 240 225 85 255) (4.2 * $scale)
        try {
            ArcLine $g 58 $arcStart $end $progressGlow 1.2 -1
            ArcLine $g 58 $arcStart $end $progress 1.2 -1
        }
        finally { $progressGlow.Dispose(); $progress.Dispose() }

        for ($i = 1; $i -lt $segments; $i++) {
            $deg = $arcStart + ($i / $segments) * $arcSweep
            $separator = PR (C 235 2 3 9) (1.35 * $scale)
            try { LinePolar $g 54 63 $deg $separator } finally { $separator.Dispose() }
        }
    } else {
        $track = PR (C 74 42 44 54) (2.2 * $scale)
        try { ArcLine $g 58 $arcStart ($arcStart + $arcSweep) $track 2.0 14 } finally { $track.Dispose() }

        $end = $arcStart + $arcSweep * ($value / 100.0)
        $glow = PR (C 72 0 235 255) (8.0 * $scale)
        $main = PR (C 255 224 255 255) (2.2 * $scale)
        try {
            ArcLine $g 58 $arcStart $end $glow 1.5 14
            ArcLine $g 58 $arcStart $end $main 1.5 14
        }
        finally { $glow.Dispose(); $main.Dispose() }
    }

    for ($i = 0; $i -le 12; $i++) {
        $deg = $arcStart + ($i / 12.0) * $arcSweep
        if (Reserved $deg 14) { continue }
        $p = PR (C 120 210 240 255) (0.9 * $scale)
        try { LinePolar $g 62 66 $deg $p } finally { $p.Dispose() }
    }
}

function CommonUi($g, [int]$value, [string]$caption, [string]$arcMode) {
    Docks $g
    $centerBrush = B (C 252 2 3 9)
    $font = [System.Drawing.Font]::new("Segoe UI Light", 35 * $scale, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $small = [System.Drawing.Font]::new("Segoe UI", 6 * $scale, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $icon = [System.Drawing.Font]::new("Segoe UI", 15 * $scale, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $text = B (C 250 226 255 255)
    $hint = B (C 235 220 252 255)
    $red = B (C 255 255 38 48)
    try {
        CircleFill $g $center $center (38 * $scale) $centerBrush
        VolumeArc $g $value $arcMode
        TextCentered $g ("{0}%" -f $value) 111 45 $font $text
        TextCentered $g "I>" 19 18 $icon $hint
        $sf = [System.Drawing.StringFormat]::new()
        try {
            $sf.Alignment = [System.Drawing.StringAlignment]::Center
            $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
            $g.DrawString("<<", $icon, $hint, [System.Drawing.RectangleF]::new(9 * $scale, 112 * $scale, 38 * $scale, 20 * $scale), $sf)
            $g.DrawString(">>", $icon, $hint, [System.Drawing.RectangleF]::new(193 * $scale, 112 * $scale, 38 * $scale, 20 * $scale), $sf)
        }
        finally { $sf.Dispose() }
        CircleFill $g $center (216 * $scale) (3.2 * $scale) $red
        TextCentered $g $caption 230 9 $small (B (C 190 145 150 165))
    }
    finally {
        $centerBrush.Dispose(); $font.Dispose(); $small.Dispose(); $icon.Dispose()
        $text.Dispose(); $hint.Dispose(); $red.Dispose()
    }
}

function Draw-CityRise($g, [int]$value) {
    Background $g
    $count = 44
    $lit = [Math]::Round($count * $value / 100.0)
    for ($i = 0; $i -lt $count; $i++) {
        $t = $i / ($count - 1)
        $deg = $arcStart + $t * $arcSweep
        if (Reserved $deg 20) { continue }
        $on = $i -lt $lit
        $col = if ($on) { Cyber $t "mix" } else { C 42 25 27 36 }
        $base = 112
        $height = if ($on) { 18 + (($i * 9 + $value) % 28) } else { 8 }
        $p = PR $col (4.5 * $scale)
        try { LinePolar $g ($base - $height) $base $deg $p } finally { $p.Dispose() }
        if ($on -and $i % 4 -eq 0) {
            $dot = B (C 210 245 255 255)
            $pt = P ($base - $height - 3) $deg
            try { CircleFill $g $pt.X $pt.Y (1.4 * $scale) $dot } finally { $dot.Dispose() }
        }
    }
}

function Draw-LiquidChrome($g, [int]$value) {
    Background $g
    $fillTop = 222 - $value * 1.55
    $clip = [System.Drawing.Drawing2D.GraphicsPath]::new()
    try {
        $clip.AddEllipse(10 * $scale, 10 * $scale, 220 * $scale, 220 * $scale)
        $state = $g.Save()
        $g.SetClip($clip)
        $gradRect = [System.Drawing.RectangleF]::new(0, $fillTop * $scale, $canvas, (230 - $fillTop) * $scale)
        $gradStart = C 220 0 235 255
        $gradEnd = C 210 255 0 210
        $brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new($gradRect, $gradStart, $gradEnd, 0)
        try {
            $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
            try {
                $pts = [System.Collections.Generic.List[System.Drawing.PointF]]::new()
                $pts.Add([System.Drawing.PointF]::new(18 * $scale, 230 * $scale))
                for ($x = 18; $x -le 222; $x += 4) {
                    $y = $fillTop + [Math]::Sin($x * .13 + $value * .11) * 7 + [Math]::Sin($x * .031) * 5
                    $pts.Add([System.Drawing.PointF]::new($x * $scale, [float]($y * $scale)))
                }
                $pts.Add([System.Drawing.PointF]::new(222 * $scale, 230 * $scale))
                $path.AddPolygon($pts.ToArray())
                $g.FillPath($brush, $path)
                $edge = PR (C 230 230 255 255) (1.4 * $scale)
                try { $g.DrawLines($edge, $pts.GetRange(1, $pts.Count - 2).ToArray()) } finally { $edge.Dispose() }
            }
            finally { $path.Dispose() }
        }
        finally { $brush.Dispose(); $g.Restore($state) }
    }
    finally { $clip.Dispose() }

    $mask = B (C 245 2 3 9)
    try { CircleFill $g $center $center (67 * $scale) $mask } finally { $mask.Dispose() }
}

function Draw-TileInvasion($g, [int]$value) {
    Background $g
    $cols = 15
    $rows = 15
    $maxTiles = [Math]::Round($cols * $rows * $value / 100.0)
    $drawn = 0
    for ($rank = 0; $rank -lt $cols + $rows; $rank++) {
        for ($cx = 0; $cx -lt $cols; $cx++) {
            $cy = $rank - $cx
            if ($cy -lt 0 -or $cy -ge $rows) { continue }
            $x = (19 + $cx * 14) * $scale
            $y = (19 + $cy * 14) * $scale
            if (-not (InDonut $x $y 72 116)) { continue }
            $deg = ([Math]::Atan2(($y / $scale) - 120, ($x / $scale) - 120) * 180.0 / [Math]::PI + 360) % 360
            if (Reserved $deg 20) { continue }
            if ($drawn -ge $maxTiles) { continue }
            $t = $drawn / [Math]::Max(1, $maxTiles)
            $col = Cyber $t "pink"
            $rect = [System.Drawing.RectangleF]::new($x - 4.7 * $scale, $y - 4.7 * $scale, 9.4 * $scale, 9.4 * $scale)
            $b = B (C 185 $col.R $col.G $col.B)
            try { $g.FillRectangle($b, $rect) } finally { $b.Dispose() }
            $drawn++
        }
    }
}

function Draw-ScanGraffiti($g, [int]$value) {
    Background $g
    $count = [Math]::Round(7 + $value * .32)
    for ($i = 0; $i -lt $count; $i++) {
        $t = $i / [Math]::Max(1, $count - 1)
        $deg = $arcStart + $t * $arcSweep
        if (Reserved $deg 21) { continue }
        $col = Cyber $t "acid"
        $a = P (78 + (($i * 5) % 11)) ($deg - 4)
        $bpt = P (114 - (($i * 7) % 10)) ($deg + 5)
        $cpt = P (95 + (($i * 13) % 18)) ($deg + 13)
        $p = PR (C 205 $col.R $col.G $col.B) (3.2 * $scale)
        $glow = PR (C 48 $col.R $col.G $col.B) (11 * $scale)
        try {
            $g.DrawLines($glow, [System.Drawing.PointF[]]@($a, $bpt, $cpt))
            $g.DrawLines($p, [System.Drawing.PointF[]]@($a, $bpt, $cpt))
        }
        finally { $p.Dispose(); $glow.Dispose() }
    }
}

function Render([string]$key, [string]$label, [string]$arcMode, [int]$value, [scriptblock]$draw) {
    $bmp = [System.Drawing.Bitmap]::new($canvas, $canvas, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    try {
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        try {
            $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
            $g.Clear([System.Drawing.Color]::Black)
            & $draw $g $value
            CommonUi $g $value $label $arcMode
        }
        finally { $g.Dispose() }
        $path = Join-Path $out ("cyberpunk-diff-{0}-{1}.png" -f $key, $value)
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $path
    }
    finally { $bmp.Dispose() }
}

$styles = @(
    @{Key="city-rise"; Label="City Rise"; Arc="mix"; Draw={ param($g,$v) Draw-CityRise $g $v }},
    @{Key="liquid-chrome"; Label="Liquid Chrome"; Arc="segmented"; Draw={ param($g,$v) Draw-LiquidChrome $g $v }},
    @{Key="tile-invasion"; Label="Tile Invasion"; Arc="pink"; Draw={ param($g,$v) Draw-TileInvasion $g $v }},
    @{Key="scan-graffiti"; Label="Scan Graffiti"; Arc="acid"; Draw={ param($g,$v) Draw-ScanGraffiti $g $v }}
)
$values = @(20, 50, 85)

Get-ChildItem -LiteralPath $out -Filter "cyberpunk-diff-*.png" | Remove-Item -Force

foreach ($s in $styles) {
    foreach ($v in $values) {
        Render $s.Key $s.Label $s.Arc $v $s.Draw | Out-Null
    }
}

$liquidBandValues = @(0, 12, 25, 38, 51, 64, 79, 92, 100)
for ($i = 0; $i -lt $liquidBandValues.Count; $i++) {
    $v = $liquidBandValues[$i]
    $tmp = Render "liquid-current-tmp" "Liquid Chrome" "segmented" $v { param($g,$value) Draw-LiquidChrome $g $value }
    $target = Join-Path $out ("cyberpunk-liquid-current-{0:d2}.png" -f $i)
    Move-Item -LiteralPath $tmp -Destination $target -Force
}

$thumb = 300
$labelH = 34
$sheet = [System.Drawing.Bitmap]::new($values.Count * $thumb, $styles.Count * ($thumb + $labelH), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
try {
    $g = [System.Drawing.Graphics]::FromImage($sheet)
    try {
        $g.Clear([System.Drawing.Color]::FromArgb(7, 7, 10))
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $font = [System.Drawing.Font]::new("Segoe UI", 13, [System.Drawing.FontStyle]::Bold)
        $brush = B ([System.Drawing.Color]::FromArgb(232, 236, 238))
        try {
            for ($r = 0; $r -lt $styles.Count; $r++) {
                for ($c = 0; $c -lt $values.Count; $c++) {
                    $path = Join-Path $out ("cyberpunk-diff-{0}-{1}.png" -f $styles[$r].Key, $values[$c])
                    $img = [System.Drawing.Bitmap]::FromFile($path)
                    try {
                        $x = $c * $thumb
                        $y = $r * ($thumb + $labelH)
                        $g.DrawString(("{0} - {1}%" -f $styles[$r].Label, $values[$c]), $font, $brush, $x + 10, $y + 7)
                        $g.DrawImage($img, $x, $y + $labelH, $thumb, $thumb)
                    }
                    finally { $img.Dispose() }
                }
            }
        }
        finally { $font.Dispose(); $brush.Dispose() }
    }
    finally { $g.Dispose() }
    $sheetPath = Join-Path $out "cyberpunk-diff-theme-contact-sheet.png"
    $sheet.Save($sheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Wrote $sheetPath"
}
finally { $sheet.Dispose() }
