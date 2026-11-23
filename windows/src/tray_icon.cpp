#include "tray_icon.h"

#include <cstdint>
#include <span>
#include <vector>

#include "../res/resource.h"
#include "log.h"

namespace {

constexpr unsigned kDimmedOpacity = 102;  // 40% of 255

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

    // The glyph, white on transparent (res/app.rc: tray.ico, drawn by make-icon.ps1 at the tray's size for each
    // display scale). Windows picks the image of this size, or scales the nearest one.
    const UniqueIcon glyph{static_cast<HICON>(
        LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_TRAY), IMAGE_ICON, size, size, LR_DEFAULTCOLOR))};
    ICONINFO glyphInfo{};
    if (!glyph || !GetIconInfo(glyph.get(), &glyphInfo)) {
        Log(L"can't draw the tray icon: can't load its glyph (error %lu)", GetLastError());
        return nullptr;
    }
    // GetIconInfo hands over copies of the icon's two bitmaps, which are ours to delete.
    const UniqueBitmap glyphBitmap{glyphInfo.hbmColor};
    const UniqueBitmap glyphMask{glyphInfo.hbmMask};

    // A 32-bit bitmap whose pixels we can reach (`bits`). The glyph's pixels are copied into it, then rewritten in
    // place as the icon's color and alpha.
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;  // negative: rows top to bottom
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    const UniqueDc dc{CreateCompatibleDC(nullptr)};
    const UniqueBitmap color{dc ? CreateDIBSection(dc.get(), &info, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr};
    if (!color || !glyphBitmap || GetDIBits(dc.get(), glyphBitmap.get(), 0, size, bits, &info, DIB_RGB_COLORS) != size) {
        Log(L"can't draw the tray icon: can't read its glyph");
        return nullptr;
    }

    // Only the glyph's alpha is kept: it becomes the alpha of the taskbar's color. Icons take straight (not
    // premultiplied) alpha: the color stays the full glyph color, and alpha alone fades it.
    const std::uint32_t ink = style.light ? 0x000000 : 0xFFFFFF;  // 0xRRGGBB: black on a light taskbar
    const unsigned opacity = dimmed ? kDimmedOpacity : 255;
    for (std::uint32_t& pixel : std::span(static_cast<std::uint32_t*>(bits), static_cast<size_t>(size) * size)) {
        const unsigned alpha = (pixel >> 24) * opacity / 255;
        pixel = alpha == 0 ? 0 : (alpha << 24) | ink;  // 0xAARRGGBB
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
