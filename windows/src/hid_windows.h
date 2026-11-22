// The Windows side of HID: finding the G635's interfaces and HID collections, and WinHidTransport,
// which carries HID++ reports over the vendor collection.
#pragma once

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "hidpp.h"
#include "win32.h"

// True when a device-interface path belongs to the G635: it contains "VID_046D&PID_0A89", in either case.
bool IsG635Path(std::wstring_view path);

// The device-interface paths of the plugged-in G635 for one interface class: GUID_DEVINTERFACE_USB_DEVICE
// for the USB device, or HidInterfaceClass() for its HID collections. Empty when it isn't plugged in.
std::expected<std::vector<std::wstring>, hidpp::Error> FindG635Interfaces(const GUID& interfaceClass);

GUID HidInterfaceClass();

// One HID collection of the G635, as reported by Windows.
struct HidCollection {
    std::wstring path;
    std::uint16_t usagePage = 0;
    std::uint16_t usage = 0;
    std::uint16_t inputLength = 0;  // report lengths in bytes, report ID included
    std::uint16_t outputLength = 0;
    std::uint16_t featureLength = 0;
    std::uint32_t error = 0;  // GetLastError if the collection couldn't be opened to read the above
};

// Every HID collection of the plugged-in G635 (it has three).
std::expected<std::vector<HidCollection>, hidpp::Error> ListG635Collections();

// True for the collection that carries HID++: usage page 0xFF43, usage 0x0202, 20-byte reports both ways.
bool IsHidppCollection(const HidCollection& collection);

// hidpp::Transport over the G635's HID++ collection, with overlapped (asynchronous) I/O so that every
// write and read can time out. `final`: nothing derives from it.
class WinHidTransport final : public hidpp::Transport {
public:
    // Finds and opens the collection. A static factory instead of a constructor, because opening can fail
    // and a constructor can't return an error.
    static std::expected<WinHidTransport, hidpp::Error> Open();

    // `override`: implements the Transport function, like Java's @Override.
    std::expected<void, hidpp::Error> Write(const hidpp::Report& report) override;
    std::expected<hidpp::Report, hidpp::Error> Read(std::chrono::steady_clock::time_point deadline) override;

    std::size_t OutputLength() const { return writeBuffer_.size(); }

private:
    WinHidTransport(UniqueHandle device, UniqueHandle writeDone, UniqueHandle readDone, std::uint16_t inputLength,
                    std::uint16_t outputLength);

    // Each member releases itself when the transport is destroyed (no destructor needed).
    UniqueHandle device_;
    UniqueHandle writeDone_;  // events signaled when an overlapped write or read completes
    UniqueHandle readDone_;
    std::vector<std::uint8_t> writeBuffer_;  // sized to the collection's report lengths
    std::vector<std::uint8_t> readBuffer_;
};
