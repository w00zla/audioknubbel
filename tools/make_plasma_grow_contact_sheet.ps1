param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$Pattern = "docs/mockups/plasma-grow-current-{0:d2}.png",
    [string]$Output = "docs/mockups/plasma-grow-current-sheet.png"
)

Add-Type -AssemblyName System.Drawing

$thumb = 360
$labelH = 34
$cols = 3
$rows = 3
$sheet = [System.Drawing.Bitmap]::new($cols * $thumb, $rows * ($thumb + $labelH), [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)

try {
    $g = [System.Drawing.Graphics]::FromImage($sheet)
    try {
        $g.Clear([System.Drawing.Color]::FromArgb(10, 10, 10))
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $font = [System.Drawing.Font]::new("Segoe UI", 15, [System.Drawing.FontStyle]::Bold)
        $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(230, 230, 230))
        try {
            for ($i = 0; $i -lt 9; $i++) {
                $path = Join-Path $Root ([string]::Format($Pattern, $i))
                if (-not (Test-Path -LiteralPath $path)) { throw "Missing image: $path" }
                $img = [System.Drawing.Bitmap]::FromFile($path)
                try {
                    $col = $i % $cols
                    $row = [Math]::Floor($i / $cols)
                    $x = $col * $thumb
                    $y = $row * ($thumb + $labelH)
                    $g.DrawString(("State {0}" -f $i), $font, $brush, $x + 12, $y + 5)
                    $g.DrawImage($img, $x, $y + $labelH, $thumb, $thumb)
                }
                finally {
                    $img.Dispose()
                }
            }
        }
        finally {
            $font.Dispose()
            $brush.Dispose()
        }
    }
    finally {
        $g.Dispose()
    }

    $outPath = Join-Path $Root $Output
    $sheet.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "Wrote $outPath"
}
finally {
    $sheet.Dispose()
}
