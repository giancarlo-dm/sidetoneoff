#include "hid_windows.h"

#include <cfgmgr32.h>
#include <hidsdi.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <utility>

#include "log.h"

namespace {

using hidpp::Error;
using hidpp::Step;

constexpr DWORD kWriteTimeoutMs = 1000;

// The milliseconds left until `deadline`, 0 if it has passed.
DWORD MillisecondsUntil(std::chrono::steady_clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    return left.count() > 0 ? static_cast<DWORD>(left.count()) : 0;
}

// Waits for an overlapped operation to finish, up to `timeoutMs`. On timeout it cancels the operation and
// waits for the cancel, so the buffer is never written after we return. Returns the bytes transferred,
// or the error.
std::expected<DWORD, Error> Finish(HANDLE device, OVERLAPPED& overlapped, DWORD timeoutMs, Step failedStep) {
    if (WaitForSingleObject(overlapped.hEvent, timeoutMs) != WAIT_OBJECT_0) {
        CancelIoEx(device, &overlapped);
        DWORD ignored = 0;
        GetOverlappedResult(device, &overlapped, &ignored, TRUE);
        return std::unexpected(Error{.step = Step::Timeout});
    }
    DWORD transferred = 0;
    if (!GetOverlappedResult(device, &overlapped, &transferred, FALSE)) {
        return std::unexpected(Error{.step = failedStep, .osError = GetLastError()});
    }
    return transferred;
}

UniqueHandle NewEvent() {
    return UniqueHandle(CreateEventW(nullptr, TRUE, FALSE, nullptr));  // manual-reset, initially not signaled
}

}  // namespace

bool IsG635Path(std::wstring_view path) {
    wchar_t match[32];
    swprintf_s(match, L"vid_%04x&pid_%04x", static_cast<unsigned>(hidpp::kLogitechVendorId),
               static_cast<unsigned>(hidpp::kG635ProductId));
    std::wstring lower(path);
    for (wchar_t& c : lower) c = static_cast<wchar_t>(std::towlower(c));
    return lower.contains(match);
}

std::expected<std::vector<std::wstring>, Error> FindG635Interfaces(const GUID& interfaceClass) {
    GUID guid = interfaceClass;  // the CM_ functions take a non-const pointer
    std::vector<wchar_t> list;
    // The list can grow between the size query and the read (a device arriving), hence the retry.
    CONFIGRET result = CR_BUFFER_SMALL;
    while (result == CR_BUFFER_SMALL) {
        ULONG size = 0;
        result = CM_Get_Device_Interface_List_SizeW(&size, &guid, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        if (result != CR_SUCCESS) break;
        list.assign(size, L'\0');
        result = CM_Get_Device_Interface_ListW(&guid, nullptr, list.data(), size, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    }
    if (result != CR_SUCCESS) {
        return std::unexpected(Error{.step = Step::NotFound, .osError = CM_MapCrToWin32Err(result, ERROR_NOT_FOUND)});
    }

    // The list is a series of null-terminated strings, ended by an empty one.
    std::vector<std::wstring> paths;
    for (const wchar_t* path = list.data(); *path != L'\0'; path += std::wcslen(path) + 1) {
        if (IsG635Path(path)) paths.emplace_back(path);
    }
    return paths;
}

GUID HidInterfaceClass() {
    GUID guid{};
    HidD_GetHidGuid(&guid);
    return guid;
}

std::expected<std::vector<HidCollection>, Error> ListG635Collections() {
    const auto paths = FindG635Interfaces(HidInterfaceClass());
    if (!paths) return std::unexpected(paths.error());

    std::vector<HidCollection> collections;
    for (const std::wstring& path : *paths) {
        HidCollection& collection = collections.emplace_back(HidCollection{.path = path});
        // Access 0 is enough to read the capabilities, and works even for the collections Windows keeps
        // open for itself (the consumer-control one, for the volume keys).
        const UniqueHandle handle = OwnFileHandle(CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                              nullptr, OPEN_EXISTING, 0, nullptr));
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (!handle || !HidD_GetPreparsedData(handle.get(), &preparsed)) {
            collection.error = GetLastError();
            continue;
        }
        HIDP_CAPS caps{};
        const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
        HidD_FreePreparsedData(preparsed);
        if (status != HIDP_STATUS_SUCCESS) {
            collection.error = ERROR_INVALID_DATA;
            continue;
        }
        collection.usagePage = caps.UsagePage;
        collection.usage = caps.Usage;
        collection.inputLength = caps.InputReportByteLength;
        collection.outputLength = caps.OutputReportByteLength;
        collection.featureLength = caps.FeatureReportByteLength;
    }
    return collections;
}

bool IsHidppCollection(const HidCollection& collection) {
    return collection.error == 0 && collection.usagePage == hidpp::kHidppUsagePage &&
           collection.usage == hidpp::kHidppUsage && collection.inputLength >= hidpp::kReportSize &&
           collection.outputLength >= hidpp::kReportSize;
}

std::expected<WinHidTransport, Error> WinHidTransport::Open() {
    const auto collections = ListG635Collections();
    if (!collections) return std::unexpected(collections.error());
    const auto target = std::ranges::find_if(*collections, IsHidppCollection);
    if (target == collections->end()) return std::unexpected(Error{.step = Step::NotFound});

    UniqueHandle device = OwnFileHandle(CreateFileW(target->path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                                    FILE_FLAG_OVERLAPPED, nullptr));
    if (!device) return std::unexpected(Error{.step = Step::Open, .osError = GetLastError()});
    UniqueHandle writeDone = NewEvent();
    UniqueHandle readDone = NewEvent();
    if (!writeDone || !readDone) return std::unexpected(Error{.step = Step::Open, .osError = GetLastError()});

    HidD_FlushQueue(device.get());  // drop reports that arrived before we opened it
    Log(L"opened %s (input %u, output %u bytes)", target->path.c_str(), target->inputLength, target->outputLength);
    // std::move hands each handle over to the transport; the locals are left empty.
    return WinHidTransport(std::move(device), std::move(writeDone), std::move(readDone), target->inputLength,
                           target->outputLength);
}

WinHidTransport::WinHidTransport(UniqueHandle device, UniqueHandle writeDone, UniqueHandle readDone,
                                 std::uint16_t inputLength, std::uint16_t outputLength)
    : device_(std::move(device)),
      writeDone_(std::move(writeDone)),
      readDone_(std::move(readDone)),
      writeBuffer_(outputLength),
      readBuffer_(inputLength) {}

std::expected<void, Error> WinHidTransport::Write(const hidpp::Report& report) {
    std::ranges::fill(writeBuffer_, std::uint8_t{0});
    std::copy_n(report.begin(), std::min(report.size(), writeBuffer_.size()), writeBuffer_.begin());

    OVERLAPPED overlapped{};
    overlapped.hEvent = writeDone_.get();
    if (!WriteFile(device_.get(), writeBuffer_.data(), static_cast<DWORD>(writeBuffer_.size()), nullptr, &overlapped)) {
        const DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) return std::unexpected(Error{.step = Step::Write, .osError = error});
    }
    const auto written = Finish(device_.get(), overlapped, kWriteTimeoutMs, Step::Write);
    if (!written) return std::unexpected(written.error());
    if (*written != writeBuffer_.size()) return std::unexpected(Error{.step = Step::Write});
    return {};
}

std::expected<hidpp::Report, Error> WinHidTransport::Read(std::chrono::steady_clock::time_point deadline) {
    OVERLAPPED overlapped{};
    overlapped.hEvent = readDone_.get();
    if (!ReadFile(device_.get(), readBuffer_.data(), static_cast<DWORD>(readBuffer_.size()), nullptr, &overlapped)) {
        const DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) return std::unexpected(Error{.step = Step::Read, .osError = error});
    }
    const auto read = Finish(device_.get(), overlapped, MillisecondsUntil(deadline), Step::Read);
    if (!read) return std::unexpected(read.error());

    hidpp::Report report{};
    std::copy_n(readBuffer_.begin(), std::min<std::size_t>(*read, report.size()), report.begin());
    return report;
}
