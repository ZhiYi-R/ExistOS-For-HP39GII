Add-Type -AssemblyName System.Drawing
$baseDir = "D:\Projects\ExistOS-For-HP39GII"
$useFont = "Noto Sans SC"
$renderHeight = 48

# Font specs
$fontSpecs = @(
    @{Name="orp_Ascii_6x12";  W=6;  H=12;},
    @{Name="VGA_Ascii_5x8";   W=5;  H=8;},
    @{Name="VGA_Ascii_6x12";  W=6;  H=12;},
    @{Name="VGA_Ascii_8x16";  W=8;  H=16;},
    @{Name="VGA_Ascii_7x14";  W=7;  H=14;}
)

$asciiCodes = 0x20..0x7E
$font = New-Object System.Drawing.Font($useFont, $renderHeight, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)

function Get-GlyphData($ch, $w, $h) {
    $margin = 12
    $bigW = 48 + $margin * 2; $bigH = 48 + $margin * 2
    $bmp = New-Object System.Drawing.Bitmap($bigW, $bigH)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::White)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.DrawString($ch, $font, [System.Drawing.Brushes]::Black, $margin, 0)

    $left = $bigW; $right = 0; $top = $bigH; $bot = 0
    for ($by = 0; $by -lt $bigH; $by++) {
        for ($bx = 0; $bx -lt $bigW; $bx++) {
            if ($bmp.GetPixel($bx, $by).R -lt 128) {
                if ($bx -lt $left) { $left = $bx }
                if ($bx -gt $right) { $right = $bx }
                if ($by -lt $top) { $top = $by }
                if ($by -gt $bot) { $bot = $by }
            }
        }
    }
    if ($left -gt $right) { $left = 0; $right = $w-1; $top = 0; $bot = $h-1 }
    $gw = $right - $left + 1; $gh = $bot - $top + 1
    $cropRect = New-Object System.Drawing.Rectangle($left, $top, $gw, $gh)
    $cropped = $bmp.Clone($cropRect, $bmp.PixelFormat)
    $scale = [Math]::Min([double]$w / $gw, [double]$h / $gh)
    $dstW = [Math]::Max(1, [int]($gw * $scale)); $dstH = [Math]::Max(1, [int]($gh * $scale))
    $tgt = New-Object System.Drawing.Bitmap($w, $h)
    $tg = [System.Drawing.Graphics]::FromImage($tgt)
    $tg.Clear([System.Drawing.Color]::White)
    $tg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    [int]$offX = ($w - $dstW) / 2; [int]$offY = ($h - $dstH) / 2
    $tg.DrawImage($cropped, $offX, $offY, $dstW, $dstH)
    $bytes = @()
    for ($y = 0; $y -lt $h; $y++) {
        $bval = 0
        for ($x = 0; $x -lt $w; $x++) {
            if ($tgt.GetPixel($x, $y).R -lt 160) { $bval = $bval -bor (0x80 -shr $x) }
        }
        $bytes += $bval
    }
    $tg.Dispose(); $tgt.Dispose(); $cropped.Dispose(); $g.Dispose(); $bmp.Dispose()
    ,$bytes
}

# Generate data arrays for each font
$allData = @{}
foreach ($spec in $fontSpecs) {
    $lines = @()
    for ($i = 0; $i -lt $asciiCodes.Length; $i++) {
        $code = $asciiCodes[$i]
        $ch = [string][char]$code
        $bytes = Get-GlyphData $ch $spec.W $spec.H
        $hexs = @(); foreach ($b in $bytes) { $hexs += "0x{0:X2}" -f $b }
        $repr = if ($code -eq 0x20) { "' '" } elseif ($code -ge 0x21 -and $code -le 0x7E) { "'$ch'" } else { "'$ch'" }
        if ($repr.Length -gt 3) { $repr = $repr.Substring(0, 3) }
        $lines += "`t" + ($hexs -join ",") + ", // $code $repr"
    }
    $allData[$spec.Name] = $lines
}
$font.Dispose()

# Now rebuild vgafont.c
$vgaOut = @(
    "const unsigned char orp_Ascii_6x12[] =              // ASCII",
    "{"
)
$vgaOut += $allData["orp_Ascii_6x12"]
$vgaOut += "};"
$vgaOut += ""
$vgaOut += "const unsigned char VGA_Ascii_5x8[] =              // ASCII"
$vgaOut += "{"
$vgaOut += $allData["VGA_Ascii_5x8"]
$vgaOut += "};"
$vgaOut += ""
$vgaOut += "const unsigned char VGA_Ascii_6x12[] =              // ASCII"
$vgaOut += "{"
$vgaOut += $allData["VGA_Ascii_6x12"]
$vgaOut += "};"
$vgaOut += ""
$vgaOut += "const unsigned char VGA_Ascii_8x16[] =              // ASCII"
$vgaOut += "{"
$vgaOut += $allData["VGA_Ascii_8x16"]
$vgaOut += "};"

$vgaOut -join "`r`n" | Set-Content "$baseDir\System\graphics\vgafont.c" -NoNewLine
Write-Host "vgafont.c rebuilt, lines: $($vgaOut.Length)"

# Rebuild kcasporing_gl.c - read it and replace the VGA_Ascii_7x14 array
$kcaPath = "$baseDir\System\applications\user\khicas\kcasporing_gl.c"
$kcaContent = Get-Content -Raw $kcaPath
# Find the start of VGA_Ascii_7x14 and everything after
$idx = $kcaContent.IndexOf("const unsigned char VGA_Ascii_7x14[]")
if ($idx -ge 0) {
    # Find the matching "};" that closes it
    $closeIdx = $kcaContent.IndexOf("};", $idx) + 2
    if ($closeIdx -ge 2) {
        $before = $kcaContent.Substring(0, $idx)
        $after = $kcaContent.Substring($closeIdx)
        # Build new VGA_Ascii_7x14 section
        $newSection = "const unsigned char VGA_Ascii_7x14[] =   {           // ASCII`r`n{`r`n"
        $newSection += ($allData["VGA_Ascii_7x14"] -join "`r`n")
        $newSection += "`r`n};"
        $kcaContent = $before + $newSection + $after
        Set-Content $kcaPath $kcaContent -NoNewLine
        Write-Host "kcasporing_gl.c VGA_Ascii_7x14 replaced"
    }
}
Write-Host "Done!"
