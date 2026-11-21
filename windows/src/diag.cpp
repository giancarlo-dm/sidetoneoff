#include "diag.h"

#include <windows.h>
#include <initguid.h>  // must come before usbiodef.h so GUID_DEVINTERFACE_USB_DEVICE is defined here
#include <usbiodef.h>
#include <strsafe.h>

#include <cstdarg>
#include <iterator>
#include <string>

#include "hid_windows.h"
#include "hidpp.h"
#include "log.h"
#include "strings.h"
#include "version.h"
#include "win32.h"

// The report is technical, so its text stays here; strings.h holds the tray's wording.

namespace {

// Appends printf-style text (wide: %s takes a wchar_t*, %hs a char*).
void Append(std::wstring& text, _Printf_format_string_ const wchar_t* format, ...) {
    wchar_t line[1024];
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(line, std::size(line), format, args);  // a truncated line is fine here
    va_end(args);
    text += line;
}

std::wstring Hex(const hidpp::Report& report) {
    std::wstring text;
    for (std::uint8_t byte : report) Append(text, text.empty() ? L"%02X" : L" %02X", byte);
    return text;
}

// The error in words, plus the system's text for the OS error code, if there is one.
std::wstring ErrorText(const hidpp::Error& error) {
    const std::string description = hidpp::Describe(error);
    std::wstring text(description.begin(), description.end());  // Describe() is plain ASCII
    if (error.osError != 0) text += L": " + SystemErrorText(error.osError);
    return text;
}

// A Transport that passes everything through to the real one and keeps a copy of the traffic for the
// report: the same interface, used to observe.
class RecordingTransport final : public hidpp::Transport {
public:
    RecordingTransport(hidpp::Transport& inner, std::wstring& traffic) : inner_(inner), traffic_(traffic) {}

    std::expected<void, hidpp::Error> Write(const hidpp::Report& report) override {
        traffic_ += L"  > " + Hex(report) + L"\n";
        return inner_.Write(report);
    }

    std::expected<hidpp::Report, hidpp::Error> Read(std::chrono::steady_clock::time_point deadline) override {
        auto report = inner_.Read(deadline);
        if (report) traffic_ += L"  < " + Hex(*report) + L"\n";
        return report;
    }

private:
    hidpp::Transport& inner_;
    std::wstring& traffic_;
};

void AppendUsbDevices(std::wstring& report) {
    report += L"USB device:\n";
    const auto paths = FindG635Interfaces(GUID_DEVINTERFACE_USB_DEVICE);
    if (!paths) {
        Append(report, L"  %s\n", ErrorText(paths.error()).c_str());
    } else if (paths->empty()) {
        report += L"  none\n";
    } else {
        for (const std::wstring& path : *paths) Append(report, L"  %s\n", path.c_str());
    }
}

void AppendHidCollections(std::wstring& report) {
    report += L"\nHID collections:\n";
    const auto collections = ListG635Collections();
    if (!collections) {
        Append(report, L"  %s\n", ErrorText(collections.error()).c_str());
        return;
    }
    if (collections->empty()) report += L"  none\n";
    for (const HidCollection& collection : *collections) {
        Append(report, L"  %s\n", collection.path.c_str());
        if (collection.error != 0) {
            Append(report, L"    can't open: %s (OS error %u)\n", SystemErrorText(collection.error).c_str(),
                   collection.error);
            continue;
        }
        Append(report, L"    usage page %04X, usage %04X, input %u, output %u, feature %u%s\n", collection.usagePage,
               collection.usage, collection.inputLength, collection.outputLength, collection.featureLength,
               IsHidppCollection(collection) ? L"   <- picked" : L"");
    }
}

// Talks to the headset: versions, the sidetone index, and the level before and after setting 0.
// Returns true when the level after reads 0.
bool AppendAttempt(std::wstring& report, std::wstring& traffic) {
    report += L"\n";
    auto transport = WinHidTransport::Open();
    if (!transport) {
        Append(report, L"Result: %s\n", ErrorText(transport.error()).c_str());
        return false;
    }
    RecordingTransport recorder{*transport, traffic};
    hidpp::Device device{recorder};

    // `if (auto x = f(); x)` declares x for the if/else only. Every step runs even if one before it failed,
    // so the report shows as much as possible.
    if (const auto version = hidpp::GetProtocolVersion(device); version) {
        Append(report, L"HID++ protocol %u.%u\n", version->major, version->minor);
    } else {
        Append(report, L"HID++ protocol: %s\n", ErrorText(version.error()).c_str());
    }
    if (const auto firmware = hidpp::GetFirmwareVersion(device); firmware) {
        Append(report, L"Firmware %hs\n", firmware->c_str());
    } else {
        Append(report, L"Firmware: %s\n", ErrorText(firmware.error()).c_str());
    }
    if (const auto index = device.FeatureIndex(hidpp::kSidetone); index) {
        Append(report, L"Sidetone feature 0x%04X at index %u\n", hidpp::kSidetone, *index);
    } else {
        Append(report, L"Sidetone feature: %s\n", ErrorText(index.error()).c_str());
    }

    report += L"\n";
    if (const auto before = hidpp::GetSidetone(device); before) {
        Append(report, L"Sidetone level before: %u\n", *before);
    } else {
        Append(report, L"Sidetone level before: %s\n", ErrorText(before.error()).c_str());
    }
    if (const auto set = hidpp::SetSidetone(device, 0); set) {
        Append(report, L"Set level 0: %zu bytes written, acknowledged by the headset\n", transport->OutputLength());
    } else {
        Append(report, L"Set level 0: %s\n", ErrorText(set.error()).c_str());
    }
    const auto after = hidpp::GetSidetone(device);
    if (after) {
        Append(report, L"Sidetone level after: %u\n", *after);
    } else {
        Append(report, L"Sidetone level after: %s\n", ErrorText(after.error()).c_str());
    }

    const bool off = after && *after == 0;
    report += off ? L"\nResult: sidetone is off.\n" : L"\nResult: sidetone is NOT off.\n";
    return off;
}

// A GUI-subsystem exe has no console of its own. Use stdout when it is redirected (a pipe or a file);
// otherwise attach to the console of the shell that started us.
bool WriteToConsole(const std::wstring& text) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    UniqueHandle console;  // only set if we open the console ourselves
    if (out == nullptr || out == INVALID_HANDLE_VALUE) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) return false;
        console = OwnFileHandle(CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            nullptr, OPEN_EXISTING, 0, nullptr));
        if (!console) return false;
        out = console.get();
    }

    DWORD written = 0;
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {  // a real console: write UTF-16 directly
        return WriteConsoleW(out, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    }
    // A pipe or a file: UTF-8.
    const int length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, utf8.data(), size, nullptr, nullptr);
    return WriteFile(out, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

}  // namespace

int RunDiag(DiagOutput output) {
    std::wstring report;
    Append(report, L"%s %hs diagnostics\n\n", ui::kAppName, SIDETONE_VERSION_STRING);
    AppendUsbDevices(report);
    AppendHidCollections(report);
    std::wstring traffic;
    const bool off = AppendAttempt(report, traffic);
    if (!traffic.empty()) report += L"\nHID++ traffic (> sent, < received):\n" + traffic;
    report += L"\nSidetone is only audible while an app uses the mic (for example, with Sound > Recording open).\n";

    Log(L"--diag: sidetone %s", off ? L"off" : L"NOT off");
    if (output == DiagOutput::Console && WriteToConsole(report)) return off ? 0 : 1;

    // The dialog, also the fallback when there's no console to write to.
    const std::wstring title = std::wstring(ui::kAppName) + L" diagnostics";
    MessageBoxW(nullptr, report.c_str(), title.c_str(), MB_OK | (off ? MB_ICONINFORMATION : MB_ICONWARNING));
    return off ? 0 : 1;
}
