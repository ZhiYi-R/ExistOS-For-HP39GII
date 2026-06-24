Add-Type -AssemblyName System.Drawing

$glyphNames = @("pi","sqrt","integ","infin","leq","geq","neq","arrow","alpha","beta","theta","Sigma","times","divide")
$glyphCodes = @(0x03C0,0x221A,0x222B,0x221E,0x2264,0x2265,0x2260,0x2192,0x03B1,0x03B2,0x03B8,0x03A3,0x00D7,0x00F7)

$sizes = @(
    @{H=8;  W=5;  Label="5x8"},
    @{H=12; W=6;  Label="6x12"},
    @{H=14; W=7;  Label="7x14"},
    @{H=16; W=8;  Label="8x16"}
)

$useFont = "Noto Sans SC"
$renderHeight = 48  # render at this pixel height, then scale down
$outDir = Join-Path $PSScriptRoot "glyphs3"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

foreach ($sz in $sizes) {
    $label = $sz.Label
    $h = $sz.H
    $w = $sz.W
    $lines = @()

    $font = New-Object System.Drawing.Font($useFont, $renderHeight, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)

    for ($idx = 0; $idx -lt 14; $idx++) {
        $code = $glyphCodes[$idx]
        $name = $glyphNames[$idx]
        $charStr = [string][char]$code

        # Render large
        $margin = 12
        $bigW = $renderHeight + $margin * 2
        $bigH = $renderHeight + $margin * 2
        $bigBmp = New-Object System.Drawing.Bitmap($bigW, $bigH)
        $bg = [System.Drawing.Graphics]::FromImage($bigBmp)
        $bg.Clear([System.Drawing.Color]::White)
        $bg.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $bg.DrawString($charStr, $font, [System.Drawing.Brushes]::Black, $margin, 0)

        # Find actual glyph bounds in the large bitmap
        $left = $bigW; $right = 0; $top = $bigH; $bot = 0
        for ($by = 0; $by -lt $bigH; $by++) {
            for ($bx = 0; $bx -lt $bigW; $bx++) {
                if ($bigBmp.GetPixel($bx, $by).R -lt 128) {
                    if ($bx -lt $left) { $left = $bx }
                    if ($bx -gt $right) { $right = $bx }
                    if ($by -lt $top) { $top = $by }
                    if ($by -gt $bot) { $bot = $by }
                }
            }
        }
        if ($left -gt $right) { $left = 0; $right = $w - 1; $top = 0; $bot = $h - 1 }
        $gh = $bot - $top + 1
        $gw = $right - $left + 1
        
        # Crop the large glyph
        $cropRect = New-Object System.Drawing.Rectangle($left, $top, $gw, $gh)
        $cropped = $bigBmp.Clone($cropRect, $bigBmp.PixelFormat)
        
        # Scale to target size preserving aspect ratio, fitting within w x h
        $scaleRatio = [Math]::Min([double]$w / $gw, [double]$h / $gh)
        $dstW = [Math]::Max(1, [int]($gw * $scaleRatio))
        $dstH = [Math]::Max(1, [int]($gh * $scaleRatio))
        
        # Create target bitmap and draw scaled glyph centered
        $tarBmp = New-Object System.Drawing.Bitmap($w, $h)
        $tg = [System.Drawing.Graphics]::FromImage($tarBmp)
        $tg.Clear([System.Drawing.Color]::White)
        $tg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $offX = [int](($w - $dstW) / 2)
        $offY = [int](($h - $dstH) / 2)
        $tg.DrawImage($cropped, $offX, $offY, $dstW, $dstH)

        # Extract bytes
        $bytes = @()
        for ($y = 0; $y -lt $h; $y++) {
            $byteVal = 0
            for ($x = 0; $x -lt $w; $x++) {
                if ($tarBmp.GetPixel($x, $y).R -lt 160) {
                    $byteVal = $byteVal -bor (0x80 -shr $x)
                }
            }
            $bytes += $byteVal
        }

        # Save debug image
        $tarBmp.Save(("${outDir}\" + $name + "_" + $label + ".png"), [System.Drawing.Imaging.ImageFormat]::Png)
        
        $hexParts = @()
        foreach ($b in $bytes) { $hexParts += ("0x{0:X2}" -f $b) }
        $lines += ("    /* " + $name.PadRight(6) + " */ " + ([string]::Join(",", $hexParts) + ","))

        $tg.Dispose(); $tarBmp.Dispose()
        $cropped.Dispose(); $bg.Dispose(); $bigBmp.Dispose()
    }
    $font.Dispose()
    Write-Host "// --- $label ---"
    foreach ($l in $lines) { Write-Host $l }
}
Write-Host "Done: $outDir"


