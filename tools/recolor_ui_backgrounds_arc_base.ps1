param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

Add-Type -AssemblyName System.Drawing

$assets = @(
    "resources/backgrounds/aurora-low.png",
    "resources/backgrounds/aurora-mid.png",
    "resources/backgrounds/aurora-high.png"
)

function Convert-HsvToRgb([double]$Hue, [double]$Saturation, [double]$Value) {
    $h = (($Hue % 360) + 360) % 360
    $c = $Value * $Saturation
    $x = $c * (1 - [Math]::Abs((($h / 60.0) % 2) - 1))
    $m = $Value - $c

    if ($h -lt 60) {
        $rp = $c; $gp = $x; $bp = 0
    } elseif ($h -lt 120) {
        $rp = $x; $gp = $c; $bp = 0
    } elseif ($h -lt 180) {
        $rp = 0; $gp = $c; $bp = $x
    } elseif ($h -lt 240) {
        $rp = 0; $gp = $x; $bp = $c
    } elseif ($h -lt 300) {
        $rp = $x; $gp = 0; $bp = $c
    } else {
        $rp = $c; $gp = 0; $bp = $x
    }

    $r = [int][Math]::Round(($rp + $m) * 255)
    $g = [int][Math]::Round(($gp + $m) * 255)
    $b = [int][Math]::Round(($bp + $m) * 255)
    return [System.Drawing.Color]::FromArgb($r, $g, $b)
}

foreach ($relative in $assets) {
    $path = Join-Path $Root $relative
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing image: $path"
    }

    $bmp = [System.Drawing.Bitmap]::FromFile($path)
    try {
        for ($y = 0; $y -lt $bmp.Height; $y++) {
            for ($x = 0; $x -lt $bmp.Width; $x++) {
                $c = $bmp.GetPixel($x, $y)
                $max = [Math]::Max($c.R, [Math]::Max($c.G, $c.B))
                $min = [Math]::Min($c.R, [Math]::Min($c.G, $c.B))
                if ($max -lt 24) { continue }

                $sat = if ($max -eq 0) { 0.0 } else { ($max - $min) / [double]$max }
                if ($sat -lt 0.18) { continue }

                $t = $x / [double]($bmp.Width - 1)
                $hue = 112.0 - (78.0 * $t) # lime on the left, amber/orange on the right
                $value = [Math]::Min(1.0, ($max / 255.0) * 1.18)
                $newSat = [Math]::Min(1.0, 0.72 + ($sat * 0.28))

                # Keep faint detail faint, but let bright filaments stay crisp.
                if ($value -lt 0.25) {
                    $newSat *= 0.72
                }

                $bmp.SetPixel($x, $y, (Convert-HsvToRgb $hue $newSat $value))
            }
        }

        $tmp = "$path.tmp.png"
        $bmp.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bmp.Dispose()
    }

    Move-Item -LiteralPath $tmp -Destination $path -Force
    Write-Host "Recolored $relative"
}
