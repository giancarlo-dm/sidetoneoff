# Draws the app icon and the tray icon's glyph with System.Drawing. build.ps1 runs it, and res/app.rc embeds both.
# Usage (Windows PowerShell 5.1 or PowerShell 7): .\windows\make-icon.ps1 -OutDir <folder>
#   -> <folder>\app.ico and <folder>\tray.ico
#
# The glyph is an original drawing of headphones (plan, decision M12): a rounded headband with filled, rounded ear
# cups. The Mac app's icons use SF Symbols' "headphones", which Apple licenses for Apple platforms only, and never
# for app icons.
#
# app.ico follows mac/make-icon.swift at every size (decision M11): a rounded tile filling 80% of the canvas, with a
# corner radius of 22.5% of its width, a vertical gradient from #665CF2 (top) to #3D2EB2 (bottom), and the white
# glyph at about half the tile's width.
# tray.ico is the glyph alone, white on transparent, at the tray's icon size for each display scale. The app recolors
# it for the taskbar's theme and dims it (tray_icon.cpp).

[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutDir)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$AppSizes = @(16, 20, 24, 32, 48, 64, 256)
# Stored PNG-compressed, which Windows reads at any size, and which keeps the exe small. The small app sizes stay
# plain bitmaps, which every icon reader takes; tray.ico is only read by the app itself.
$AppPngSizes = @(48, 64, 256)
$TraySizes = @(16, 20, 24, 28, 32, 36, 40, 48, 56, 64)  # the small-icon size at 100%, 125%, … 400% display scale
$TopColor = [System.Drawing.Color]::FromArgb(0x66, 0x5C, 0xF2)
$BottomColor = [System.Drawing.Color]::FromArgb(0x3D, 0x2E, 0xB2)

function New-RoundedRectangle([float]$X, [float]$Y, [float]$Width, [float]$Height, [float]$Radius) {
    $d = $Radius * 2
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($X, $Y, $d, $d, 180, 90)
    $path.AddArc($X + $Width - $d, $Y, $d, $d, 270, 90)
    $path.AddArc($X + $Width - $d, $Y + $Height - $d, $d, $d, 0, 90)
    $path.AddArc($X, $Y + $Height - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

# The headphones, in the square ($Left, $Top, $Size). The drawing is laid out on a 24-unit grid and spans 2.5 to
# 21.5 on both axes, so it sits centered with a margin.
function Draw-Headphones([System.Drawing.Graphics]$Graphics, [float]$Left, [float]$Top, [float]$Size,
                         [System.Drawing.Color]$Color) {
    $k = $Size / 24  # pixels per grid unit
    $band = New-Object System.Drawing.Drawing2D.GraphicsPath
    $pen = New-Object System.Drawing.Pen $Color, (2 * $k)  # 2 units: about the Mac symbol's semibold weight (M8)
    $brush = New-Object System.Drawing.SolidBrush $Color
    $cups = @()
    try {
        # Headband: a half circle (center 12,12, radius 8.5), continued straight down into the ear cups.
        $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $band.AddLine($Left + 3.5 * $k, $Top + 16 * $k, $Left + 3.5 * $k, $Top + 12 * $k)
        $band.AddArc($Left + 3.5 * $k, $Top + 3.5 * $k, 17 * $k, 17 * $k, 180, 180)
        $band.AddLine($Left + 20.5 * $k, $Top + 12 * $k, $Left + 20.5 * $k, $Top + 16 * $k)
        $Graphics.DrawPath($pen, $band)
        # Ear cups: solid rounded rectangles, flush with the band's outer edge.
        foreach ($x in 2.5, 16) {
            $cup = New-RoundedRectangle ($Left + $x * $k) ($Top + 13.5 * $k) (5.5 * $k) (8 * $k) (2.5 * $k)
            $cups += $cup
            $Graphics.FillPath($brush, $cup)
        }
    }
    finally {
        foreach ($disposable in @($band, $pen, $brush) + $cups) { $disposable.Dispose() }
    }
}

function New-Canvas([int]$Size) {
    return New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function Start-Drawing([System.Drawing.Bitmap]$Bitmap) {
    $graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    return $graphics
}

# The app icon at one size, drawn at that size (not scaled down from a bigger one).
function New-AppIconBitmap([int]$Size) {
    $bitmap = New-Canvas $Size
    $graphics = Start-Drawing $bitmap
    $tilePath = $null; $tileBrush = $null
    try {
        $tile = $Size * 0.8
        $margin = $Size * 0.1
        $tilePath = New-RoundedRectangle $margin $margin $tile $tile ($tile * 0.225)
        # GDI+ wraps a gradient around at its brush's edges, which would tint the tile's top row with the bottom
        # color. A brush 1 px taller than the tile at each end keeps both rows clean.
        $brushArea = New-Object System.Drawing.RectangleF $margin, ($margin - 1), $tile, ($tile + 2)
        $tileBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $brushArea, $TopColor, $BottomColor,
            ([System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
        $graphics.FillPath($tileBrush, $tilePath)
        # The 24-unit grid at 60% of the tile puts the headphones (19 units across) at about half the tile.
        $glyph = $tile * 0.6
        Draw-Headphones $graphics (($Size - $glyph) / 2) (($Size - $glyph) / 2) $glyph ([System.Drawing.Color]::White)
    }
    finally {
        foreach ($disposable in $tileBrush, $tilePath, $graphics) { if ($null -ne $disposable) { $disposable.Dispose() } }
    }
    return $bitmap
}

# The tray glyph at one size: white on transparent, filling the icon like the font glyph did.
function New-TrayGlyphBitmap([int]$Size) {
    $bitmap = New-Canvas $Size
    $graphics = Start-Drawing $bitmap
    try { Draw-Headphones $graphics 0 0 $Size ([System.Drawing.Color]::White) }
    finally { $graphics.Dispose() }
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

# Writes an .ico: a 6-byte header, one 16-byte entry per image, then the images. $NewBitmap draws one size.
function Write-Ico([string]$Path, [int[]]$Sizes, [int[]]$PngSizes, [scriptblock]$NewBitmap) {
    $images = New-Object 'System.Collections.Generic.List[byte[]]'
    foreach ($size in $Sizes) {
        $bitmap = & $NewBitmap $size
        try {
            if ($PngSizes -contains $size) { $images.Add((Get-PngBytes $bitmap)) }
            else { $images.Add((Get-IcoBitmapBytes $bitmap)) }
        }
        finally { $bitmap.Dispose() }
    }

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
    [System.IO.File]::WriteAllBytes($Path, $stream.ToArray())
    Write-Host "==> $(Split-Path $Path -Leaf): $($Sizes -join ', ') px, $([math]::Ceiling($stream.Length / 1KB)) KB"
}

$folder = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
Write-Ico (Join-Path $folder 'app.ico') $AppSizes $AppPngSizes { param($size) New-AppIconBitmap $size }
Write-Ico (Join-Path $folder 'tray.ico') $TraySizes $TraySizes { param($size) New-TrayGlyphBitmap $size }
