# Draws the app icon, an .ico with every size Windows asks for, with System.Drawing. build.ps1 runs it and
# res/app.rc embeds the result in the exe.
# Usage (Windows PowerShell 5.1 or PowerShell 7): .\windows\make-icon.ps1 -Out <path\app.ico>
#
# The design follows mac/make-icon.swift: a rounded tile filling 80% of the canvas, with a corner radius of
# 22.5% of its width, a vertical gradient from #665CF2 (top) to #3D2EB2 (bottom), and the white headphones
# glyph at half the tile's width. SF Symbols' license only covers Apple platforms, so the glyph is Segoe Fluent
# Icons U+E7F6 (Segoe MDL2 Assets on Windows 10). Both are lighter than the Mac's semibold symbol, so the glyph
# also gets a white outline that thickens it (plan, decision M8).
#
# At 16, 20 and 24 px those proportions leave a glyph too small to read, so these sizes use a small-size layout
# (plan, decision M11): the tile fills the icon, and the glyph, at 75% of it, is hinted onto whole pixels.

[CmdletBinding()]
param([Parameter(Mandatory)][string]$Out)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$Sizes = @(16, 20, 24, 32, 48, 64, 256)
$SmallLayoutSizes = @(16, 20, 24)  # decision M11
# Stored PNG-compressed, which Windows reads at any size, and which keeps the exe small. The small sizes stay
# plain bitmaps, which every icon reader takes.
$PngSizes = @(48, 64, 256)
$Glyph = [string][char]0xE7F6
$TopColor = [System.Drawing.Color]::FromArgb(0x66, 0x5C, 0xF2)
$BottomColor = [System.Drawing.Color]::FromArgb(0x3D, 0x2E, 0xB2)
$OutlineWidth = 0.03  # as a fraction of the glyph's em size (M8)

function Get-IconFontFamily {
    foreach ($name in 'Segoe Fluent Icons', 'Segoe MDL2 Assets') {
        try { return New-Object System.Drawing.FontFamily $name }
        catch { Write-Verbose "$name is not installed" }
    }
    throw 'Neither Segoe Fluent Icons nor Segoe MDL2 Assets is installed.'
}

function New-RoundedRectangle([System.Drawing.RectangleF]$Rect, [float]$Radius) {
    $d = $Radius * 2
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($Rect.X, $Rect.Y, $d, $d, 180, 90)
    $path.AddArc($Rect.Right - $d, $Rect.Y, $d, $d, 270, 90)
    $path.AddArc($Rect.Right - $d, $Rect.Bottom - $d, $d, $d, 0, 90)
    $path.AddArc($Rect.X, $Rect.Bottom - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

# The icon at one size, drawn at that size (not scaled down from a bigger one).
function New-IconBitmap([int]$Size, [System.Drawing.FontFamily]$FontFamily) {
    $small = $SmallLayoutSizes -contains $Size
    $bitmap = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $tilePath = $null; $tileBrush = $null; $font = $null; $glyphPath = $null; $matrix = $null; $outline = $null
    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $tileWidth = if ($small) { $Size } else { $Size * 0.8 }
        $margin = ($Size - $tileWidth) / 2
        $tile = New-Object System.Drawing.RectangleF $margin, $margin, $tileWidth, $tileWidth
        $tilePath = New-RoundedRectangle $tile ($tile.Width * 0.225)
        # GDI+ wraps a gradient around at its brush's edges, which would tint the tile's top row with the bottom
        # color. A brush 1 px taller than the tile at each end keeps both rows clean.
        $brushArea = New-Object System.Drawing.RectangleF $tile.X, ($tile.Y - 1), $tile.Width, ($tile.Height + 2)
        $tileBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $brushArea, $TopColor, $BottomColor,
            ([System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
        $graphics.FillPath($tileBrush, $tilePath)

        if ($small) {
            # The font's hinting puts each stroke on whole pixels, given a whole-pixel em size and position.
            # 1-bit up to 20 px, where smoothing would blur the 1 px strokes; smoothed at 24 px, where 1-bit
            # shows steps on the arcs. No outline: it would undo the hinting.
            $emSize = [math]::Round($Size * 0.75)
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::None
            $graphics.TextRenderingHint = if ($Size -le 20) { [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit }
                                          else { [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit }
            $font = New-Object System.Drawing.Font $FontFamily, $emSize, ([System.Drawing.FontStyle]::Regular),
                ([System.Drawing.GraphicsUnit]::Pixel)
            $corner = [math]::Floor(($Size - $emSize) / 2)  # the font centers the glyph in its em box
            $graphics.DrawString($Glyph, $font, [System.Drawing.Brushes]::White, $corner, $corner,
                [System.Drawing.StringFormat]::GenericTypographic)
            return $bitmap
        }

        $emSize = $tile.Width * 0.5
        $glyphPath = New-Object System.Drawing.Drawing2D.GraphicsPath
        $glyphPath.AddString($Glyph, $FontFamily, [int][System.Drawing.FontStyle]::Regular, $emSize,
            (New-Object System.Drawing.PointF 0, 0), [System.Drawing.StringFormat]::GenericTypographic)
        # Center the glyph's outline on the canvas, as the Mac centers its symbol.
        $bounds = $glyphPath.GetBounds()
        $matrix = New-Object System.Drawing.Drawing2D.Matrix
        $matrix.Translate(($Size - $bounds.Width) / 2 - $bounds.X, ($Size - $bounds.Height) / 2 - $bounds.Y)
        $glyphPath.Transform($matrix)
        $graphics.FillPath([System.Drawing.Brushes]::White, $glyphPath)
        $outline = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), ($emSize * $OutlineWidth)
        $outline.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $graphics.DrawPath($outline, $glyphPath)
    }
    finally {
        foreach ($disposable in $outline, $matrix, $glyphPath, $font, $tileBrush, $tilePath, $graphics) {
            if ($null -ne $disposable) { $disposable.Dispose() }
        }
    }
    return $bitmap
}

function Get-PngBytes([System.Drawing.Bitmap]$Bitmap) {
    $stream = New-Object System.IO.MemoryStream
    $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    return , $stream.ToArray()  # the comma keeps PowerShell from unrolling the array into its bytes
}

# A bitmap the way an .ico stores one: a BITMAPINFOHEADER with the height doubled (the color image, then the
# mask), the 32-bit BGRA rows bottom to top, then a 1-bit mask where 1 = transparent. Windows draws 32-bit
# icons from their alpha; the mask is for readers that ignore alpha.
function Get-IcoBitmapBytes([System.Drawing.Bitmap]$Bitmap) {
    $size = $Bitmap.Width
    $data = $Bitmap.LockBits((New-Object System.Drawing.Rectangle 0, 0, $size, $size),
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $stride = $data.Stride
        $pixels = New-Object byte[] ($stride * $size)
        [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
    }
    finally { $Bitmap.UnlockBits($data) }

    $maskStride = [int][math]::Ceiling($size / 32) * 4  # mask rows are padded to 4 bytes
    $mask = New-Object byte[] ($maskStride * $size)
    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter $stream
    $writer.Write([uint32]40)                                 # biSize
    $writer.Write([int32]$size)                               # biWidth
    $writer.Write([int32]($size * 2))                         # biHeight: image + mask
    $writer.Write([uint16]1)                                  # biPlanes
    $writer.Write([uint16]32)                                 # biBitCount
    $writer.Write([uint32]0)                                  # biCompression: BI_RGB
    $writer.Write([uint32]($size * 4 * $size + $mask.Length)) # biSizeImage
    $writer.Write([int32]0); $writer.Write([int32]0)          # biXPelsPerMeter, biYPelsPerMeter
    $writer.Write([uint32]0); $writer.Write([uint32]0)        # biClrUsed, biClrImportant
    for ($y = $size - 1; $y -ge 0; $y--) {
        $writer.Write($pixels, $y * $stride, $size * 4)
        $maskRow = ($size - 1 - $y) * $maskStride
        for ($x = 0; $x -lt $size; $x++) {
            if ($pixels[$y * $stride + $x * 4 + 3] -lt 128) {  # alpha: mostly transparent
                $mask[$maskRow + ($x -shr 3)] = $mask[$maskRow + ($x -shr 3)] -bor (0x80 -shr ($x -band 7))
            }
        }
    }
    $writer.Write($mask)
    $writer.Flush()
    return , $stream.ToArray()
}

$fontFamily = Get-IconFontFamily
$images = New-Object 'System.Collections.Generic.List[byte[]]'
foreach ($size in $Sizes) {
    $bitmap = New-IconBitmap $size $fontFamily
    try {
        if ($PngSizes -contains $size) { $images.Add((Get-PngBytes $bitmap)) }
        else { $images.Add((Get-IcoBitmapBytes $bitmap)) }
    }
    finally { $bitmap.Dispose() }
}

# The .ico file: a 6-byte header, one 16-byte entry per image, then the images.
$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $stream
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$Sizes.Count)  # reserved, 1 = icon, count
$offset = 6 + 16 * $Sizes.Count
for ($i = 0; $i -lt $Sizes.Count; $i++) {
    $dimension = if ($Sizes[$i] -ge 256) { 0 } else { $Sizes[$i] }  # one byte each; 0 means 256
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
    $writer.Write([byte]0); $writer.Write([byte]0)        # no palette, reserved
    $writer.Write([uint16]1); $writer.Write([uint16]32)   # planes, bits per pixel
    $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($image in $images) { $writer.Write($image) }
$writer.Flush()

$path = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out)
[System.IO.File]::WriteAllBytes($path, $stream.ToArray())
Write-Host "==> $(Split-Path $path -Leaf): $($Sizes -join ', ') px, $([math]::Ceiling($stream.Length / 1KB)) KB, $($fontFamily.Name)"
$fontFamily.Dispose()
