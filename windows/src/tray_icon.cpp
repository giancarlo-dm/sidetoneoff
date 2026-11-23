#include "tray_icon.h"

#include <cstdint>
#include <span>
#include <vector>

#include "log.h"

namespace {

constexpr wchar_t kHeadphones = L'\xE7F6';  // "Headphone" in both icon fonts
// Windows 11's icon font first, then Windows 10's.
constexpr const wchar_t* kIconFonts[] = {L"Segoe Fluent Icons", L"Segoe MDL2 Assets"};
constexpr unsigned kDimmedOpacity = 102;  // 40% of 255

// Selects `object` into `dc` for as long as it lives, then selects the previous object back.
class SelectedObject {
public:
    SelectedObject(HDC dc, HGDIOBJ object) : dc_(dc), previous_(SelectObject(dc, object)) {}
    ~SelectedObject() { SelectObject(dc_, previous_); }

    SelectedObject(const SelectedObject&) = delete;
    SelectedObject& operator=(const SelectedObject&) = delete;

private:
    HDC dc_;
    HGDIOBJ previous_;
};

// The first icon font that has the headphones glyph, with an em size of `size` pixels. GDI quietly puts
// another font (Arial) in place of one that isn't installed, so the glyph itself is checked.
UniqueFont CreateIconFont(HDC dc, int size) {
    for (const wchar_t* face : kIconFonts) {
        // A negative height is the em size. ANTIALIASED_QUALITY: grayscale smoothing, never ClearType's colors.
        UniqueFont font{CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS,
                                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face)};
        if (!font) continue;
        WORD glyph = 0xFFFF;
        {
            const SelectedObject selected(dc, font.get());
            GetGlyphIndicesW(dc, &kHeadphones, 1, &glyph, GGI_MARK_NONEXISTING_GLYPHS);
        }
        if (glyph != 0xFFFF) return font;
    }
    return nullptr;
}

}  // namespace

TaskbarStyle CurrentTaskbarStyle() {
    // The taskbar is per-monitor DPI aware, so its window has the DPI of the monitor it is on. Without
    // Explorer there is no taskbar, and the system DPI will do.
    const HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = taskbar ? GetDpiForWindow(taskbar) : 0;
    if (dpi == 0) dpi = GetDpiForSystem();

    // Settings > Personalization > Colors, "Choose your default Windows mode": 1 = light. Windows 10 before
    // 1903 has no such value, and a dark taskbar only.
    DWORD light = 0;
    DWORD bytes = sizeof(light);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &bytes) != ERROR_SUCCESS) {
        light = 0;
    }
    return TaskbarStyle{.iconSize = GetSystemMetricsForDpi(SM_CXSMICON, dpi), .light = light != 0};
}

UniqueIcon RenderTrayIcon(const TaskbarStyle& style, bool dimmed) {
    const int size = style.iconSize;
    const UniqueDc dc{CreateCompatibleDC(nullptr)};
    if (!dc) {
        Log(L"can't draw the tray icon: no DC (error %lu)", GetLastError());
        return nullptr;
    }

    // A 32-bit bitmap whose pixels we can reach (`bits`). The glyph is drawn into it, then each pixel is
    // rewritten in place as the icon's color and alpha.
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;  // negative: rows top to bottom
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    const UniqueBitmap color{CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &bits, nullptr, 0)};
    const UniqueFont font = CreateIconFont(dc.get(), size);
    if (!color || !font) {
        Log(L"can't draw the tray icon: %s", color ? L"no icon font has the headphones glyph" : L"no bitmap");
        return nullptr;
    }

    {
        const SelectedObject selectedBitmap(dc.get(), color.get());
        const SelectedObject selectedFont(dc.get(), font.get());
        // White on black, smoothed in grays: each pixel's brightness says how much of it the glyph covers.
        SetTextColor(dc.get(), RGB(255, 255, 255));
        SetBkColor(dc.get(), RGB(0, 0, 0));
        // Both fonts' em box is exactly the icon (ascent = em size, no descent, no leading), so the glyph is
        // drawn at the top-left corner and lands centered, the way the font's designers placed it.
        const RECT all{0, 0, size, size};
        ExtTextOutW(dc.get(), 0, 0, ETO_OPAQUE, &all, &kHeadphones, 1, nullptr);
        GdiFlush();  // GDI may still be drawing in the background; finish before reading the pixels
    }

    // Coverage becomes alpha. Icons take straight (not premultiplied) alpha: the color stays the full
    // glyph color, and alpha alone fades it.
    const std::uint32_t glyphColor = style.light ? 0x000000 : 0xFFFFFF;  // 0xRRGGBB
    const unsigned opacity = dimmed ? kDimmedOpacity : 255;
    for (std::uint32_t& pixel : std::span(static_cast<std::uint32_t*>(bits), static_cast<size_t>(size) * size)) {
        const unsigned coverage = pixel & 0xFF;  // the blue byte; gray smoothing makes red = green = blue
        const unsigned alpha = coverage * opacity / 255;
        pixel = alpha == 0 ? 0 : (alpha << 24) | glyphColor;  // 0xAARRGGBB
    }

    // A 32-bit icon is drawn from its alpha, and its 1-bit mask goes unused, but CreateIconIndirect requires
    // one: all zeros. Mask rows are padded to 2 bytes.
    const std::vector<BYTE> zeros(static_cast<size_t>((size + 15) / 16) * 2 * size, 0);
    const UniqueBitmap mask{CreateBitmap(size, size, 1, 1, zeros.data())};
    ICONINFO iconInfo{.fIcon = TRUE, .hbmMask = mask.get(), .hbmColor = color.get()};
    UniqueIcon icon{mask ? CreateIconIndirect(&iconInfo) : nullptr};  // the icon gets its own copy of both bitmaps
    if (!icon) Log(L"can't draw the tray icon: CreateIconIndirect failed (error %lu)", GetLastError());
    return icon;
}
