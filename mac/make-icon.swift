// Renders the SidetoneOff app icon into an .iconset folder for iconutil.
// Usage: swift make-icon.swift <output.iconset>

import AppKit

let symbolName = "headphones"
let topColor = NSColor(calibratedRed: 0.40, green: 0.36, blue: 0.95, alpha: 1)
let bottomColor = NSColor(calibratedRed: 0.24, green: 0.18, blue: 0.70, alpha: 1)

let outDir = CommandLine.arguments[1]
try FileManager.default.createDirectory(atPath: outDir, withIntermediateDirectories: true)

func render(_ px: Int) -> Data {
    let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    rep.size = NSSize(width: px, height: px)

    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)

    // macOS icon grid: the rounded square fills ~80% of the canvas.
    let canvas = CGFloat(px)
    let tile = NSRect(x: canvas * 0.1, y: canvas * 0.1, width: canvas * 0.8, height: canvas * 0.8)
    let tilePath = NSBezierPath(roundedRect: tile, xRadius: tile.width * 0.225, yRadius: tile.width * 0.225)
    NSGradient(starting: topColor, ending: bottomColor)!.draw(in: tilePath, angle: -90)

    let config = NSImage.SymbolConfiguration(pointSize: tile.width * 0.5, weight: .semibold)
    if let symbol = NSImage(systemSymbolName: symbolName, accessibilityDescription: nil)?
        .withSymbolConfiguration(config) {
        let white = NSImage(size: symbol.size, flipped: false) { rect in
            symbol.draw(in: rect)
            NSColor.white.set()
            rect.fill(using: .sourceAtop)
            return true
        }
        let size = white.size
        white.draw(in: NSRect(x: (canvas - size.width) / 2, y: (canvas - size.height) / 2,
                              width: size.width, height: size.height))
    }

    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

let sizes: [(String, Int)] = [
    ("16x16", 16), ("16x16@2x", 32), ("32x32", 32), ("32x32@2x", 64),
    ("128x128", 128), ("128x128@2x", 256), ("256x256", 256), ("256x256@2x", 512),
    ("512x512", 512), ("512x512@2x", 1024),
]
for (name, px) in sizes {
    try render(px).write(to: URL(fileURLWithPath: "\(outDir)/icon_\(name).png"))
}
