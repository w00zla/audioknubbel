param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$Output = "src/ui_backgrounds.cpp"
)

Add-Type -AssemblyName System.Drawing

function New-AssetRange([string]$NamePrefix, [string]$FilePrefix, [bool]$Downtone, [bool]$MaskIconPads) {
    $items = @()
    for ($i = 0; $i -lt 15; $i++) {
        $items += @{
            Name = ("{0}_{1:d2}" -f $NamePrefix, $i)
            Path = ("docs/mockups/{0}-15-{1:d2}.png" -f $FilePrefix, $i)
            Downtone = $Downtone
            MaskIconPads = $MaskIconPads
        }
    }
    return $items
}

$assets = @()
$assets += New-AssetRange "ui_bg" "plasma-grow" $true $true
$assets += New-AssetRange "ui_bg_cp" "cyberpunk-liquid" $false $false
$assets += New-AssetRange "ui_bg_bios" "bios-crt" $false $false
$assets += New-AssetRange "ui_bg_space" "deep-space" $false $false
$assets += New-AssetRange "ui_bg_lava" "lava-core" $false $false
$assets += New-AssetRange "ui_bg_ice" "ice-neon" $false $false

$width = 240
$height = 240
$outPath = Join-Path $Root $Output
$lines = [System.Collections.Generic.List[string]]::new()

function Add-Line([string]$Text = "") {
    $script:lines.Add($Text)
}

function Add-Header {
    Add-Line '#include "ui_backgrounds.h"'
    Add-Line ""
    Add-Line "/* Generated from docs/mockups/*-15-*.png"
    Add-Line "   by tools/convert_ui_background_pngs.ps1."
    Add-Line "   RGB565 little-endian for LVGL true-color images. */"
    Add-Line ""
}

function Darken-Pixel([System.Drawing.Bitmap]$Bitmap, [int]$X, [int]$Y, [double]$Keep) {
    $c = $Bitmap.GetPixel($X, $Y)
    $nr = [int][Math]::Round($c.R * $Keep)
    $ng = [int][Math]::Round($c.G * $Keep)
    $nb = [int][Math]::Round($c.B * $Keep)
    $Bitmap.SetPixel($X, $Y, [System.Drawing.Color]::FromArgb($nr, $ng, $nb))
}

function Apply-Ui-Mask([System.Drawing.Bitmap]$Bitmap, [bool]$MaskIconPads) {
    $cx = ($width - 1) / 2.0
    $cy = ($height - 1) / 2.0

    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            $dx = $x - $cx
            $dy = $y - $cy
            $r = [Math]::Sqrt(($dx * $dx) + ($dy * $dy))

            # Remove generated center UI, static tick arc, and subtle inner outline.
            if ($r -lt 84.0) {
                Darken-Pixel $Bitmap $x $y 0.0
                continue
            }

            if ($MaskIconPads) {
                # Plasma keeps media hints; firmware draws the icons/dot itself.
                $inTopPad = ($x -ge 94 -and $x -le 146 -and $y -le 54)
                $inLeftPad = ($x -le 56 -and $y -ge 92 -and $y -le 148)
                $inRightPad = ($x -ge 184 -and $y -ge 92 -and $y -le 148)
                $inBottomPad = ($x -ge 94 -and $x -le 146 -and $y -ge 186)
                if ($inTopPad -or $inLeftPad -or $inRightPad -or $inBottomPad) {
                    Darken-Pixel $Bitmap $x $y 0.0
                }
            }
        }
    }
}

function Apply-Plasma-Downtone([System.Drawing.Bitmap]$Bitmap) {
    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            $c = $Bitmap.GetPixel($x, $y)
            $max = [Math]::Max($c.R, [Math]::Max($c.G, $c.B))
            if ($max -lt 18) { continue }

            $luma = (0.2126 * $c.R) + (0.7152 * $c.G) + (0.0722 * $c.B)
            $highlight = [Math]::Max(0.0, ($max - 120.0) / 135.0)
            $scale = 0.72 - (0.12 * $highlight)
            $desat = 0.14 + (0.08 * $highlight)

            $nr = (($c.R * (1.0 - $desat)) + ($luma * $desat)) * $scale
            $ng = (($c.G * (1.0 - $desat)) + ($luma * $desat)) * $scale
            $nb = (($c.B * (1.0 - $desat)) + ($luma * $desat)) * $scale

            $Bitmap.SetPixel(
                $x,
                $y,
                [System.Drawing.Color]::FromArgb(
                    [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($nr))),
                    [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($ng))),
                    [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($nb)))
                )
            )
        }
    }
}

function Convert-Image([string]$Name, [string]$RelativePath, [bool]$Downtone, [bool]$MaskIconPads) {
    $path = Join-Path $Root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing image: $path"
    }

    $src = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $bmp = [System.Drawing.Bitmap]::new($width, $height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            try {
                $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

                $scale = [Math]::Max($width / $src.Width, $height / $src.Height)
                $drawW = [int][Math]::Round($src.Width * $scale)
                $drawH = [int][Math]::Round($src.Height * $scale)
                $drawX = [int][Math]::Floor(($width - $drawW) / 2)
                $drawY = [int][Math]::Floor(($height - $drawH) / 2)
                $g.Clear([System.Drawing.Color]::Black)
                $g.DrawImage($src, $drawX, $drawY, $drawW, $drawH)
            }
            finally {
                $g.Dispose()
            }

            Apply-Ui-Mask $bmp $MaskIconPads
            if ($Downtone) {
                Apply-Plasma-Downtone $bmp
            }

            $attr = "LV_ATTRIBUTE_IMG_$($Name.ToUpperInvariant())"
            Add-Line "#ifndef $attr"
            Add-Line "#define $attr"
            Add-Line "#endif"
            Add-Line "const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST $attr uint8_t ${Name}_map[] = {"
            for ($y = 0; $y -lt $height; $y++) {
                $bytes = [System.Collections.Generic.List[string]]::new()
                for ($x = 0; $x -lt $width; $x++) {
                    $c = $bmp.GetPixel($x, $y)
                    $rgb565 = (($c.R -band 0xF8) -shl 8) -bor (($c.G -band 0xFC) -shl 3) -bor ($c.B -shr 3)
                    $bytes.Add(("0x{0:x2},0x{1:x2}" -f ($rgb565 -band 0xFF), (($rgb565 -shr 8) -band 0xFF)))
                }
                Add-Line ("    " + ($bytes -join ",") + ",")
            }
            Add-Line "};"
            Add-Line ""
            Add-Line "const lv_img_dsc_t $Name = {"
            Add-Line "    { LV_IMG_CF_TRUE_COLOR, 0, 0, $width, $height },"
            Add-Line "    $($width * $height * 2),"
            Add-Line "    ${Name}_map,"
            Add-Line "};"
            Add-Line ""
        }
        finally {
            $bmp.Dispose()
        }
    }
    finally {
        $src.Dispose()
    }
}

Add-Header
foreach ($asset in $assets) {
    Convert-Image $asset.Name $asset.Path $asset.Downtone $asset.MaskIconPads
}

Set-Content -LiteralPath $outPath -Value $lines -Encoding ASCII
Write-Host "Wrote $outPath"
