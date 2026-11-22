#include "hidpp.h"

#include <algorithm>
#include <cstdio>

namespace hidpp {
namespace {  // private to this file

// Request and answer layout: [report ID, device index, feature index, function << 4 | software ID, params...]
constexpr std::uint8_t kLongReportId = 0x11;
constexpr std::uint8_t kDeviceIndex = 0xFF;        // the device itself (wired, no receiver)
constexpr std::uint8_t kErrorFeatureIndex = 0xFF;  // an error answer: [0x11, 0xFF, 0xFF, index, fn|sw, code]
// Our tag in each request; the headset echoes it back. 0x0A makes the set-sidetone request
// byte-identical to the spec's `11 FF 07 1A 00`.
constexpr std::uint8_t kSoftwareId = 0x0A;

// Function IDs, per feature.
constexpr std::uint8_t kRootGetFeature = 0;
constexpr std::uint8_t kRootGetProtocolVersion = 1;
constexpr std::uint8_t kFirmwareGetEntityCount = 0;
constexpr std::uint8_t kFirmwareGetInfo = 1;
constexpr std::uint8_t kSidetoneGetLevel = 0;
constexpr std::uint8_t kSidetoneSetLevel = 1;

const char* StepText(Step step) {
    switch (step) {
    case Step::NotFound: return "G635 not found";
    case Step::Open: return "couldn't open the G635";
    case Step::Write: return "couldn't send to the headset";
    case Step::Read: return "couldn't read from the headset";
    case Step::Timeout: return "no answer from the headset within 1 s";
    case Step::HidppError: return "the headset returned HID++ error";
    case Step::NoFeature: return "the headset doesn't have this feature";
    case Step::Verify: return "the headset reads back";
    }
    return "unknown error";
}

// HID++ 2.0 error codes.
const char* HidppErrorName(std::uint8_t code) {
    switch (code) {
    case 1: return "Unknown";
    case 2: return "InvalidArgument";
    case 3: return "OutOfRange";
    case 4: return "HardwareError";
    case 5: return "LogitechInternal";
    case 6: return "InvalidFeatureIndex";
    case 7: return "InvalidFunctionId";
    case 8: return "Busy";
    case 9: return "Unsupported";
    default: return "?";
    }
}

// Tags an error with the feature being called, unless it already names one.
Error ForFeature(Error error, std::uint16_t featureId) {
    if (error.featureId == 0) error.featureId = featureId;
    return error;
}

}  // namespace

std::string Describe(const Error& error) {
    std::string text = StepText(error.step);
    char detail[64];
    if (error.step == Step::HidppError) {
        std::snprintf(detail, sizeof detail, " 0x%02X %s", error.hidppError, HidppErrorName(error.hidppError));
        text += detail;
    }
    if (error.step == Step::Verify) {
        std::snprintf(detail, sizeof detail, " %u", static_cast<unsigned>(error.readBack));
        text += detail;
    }
    if (error.featureId != 0) {
        std::snprintf(detail, sizeof detail, " (feature 0x%04X)", error.featureId);
        text += detail;
    }
    if (error.osError != 0) {
        std::snprintf(detail, sizeof detail, " (OS error %u)", static_cast<unsigned>(error.osError));
        text += detail;
    }
    return text;
}

std::expected<Report, Error> Device::Call(std::uint16_t featureId, std::uint8_t function, const Params& params) {
    const auto index = FeatureIndex(featureId);
    if (!index) return std::unexpected(index.error());  // `std::unexpected(e)` = return the error
    return CallIndex(*index, featureId, function, params);
}

std::expected<std::uint8_t, Error> Device::FeatureIndex(std::uint16_t featureId) {
    if (featureId == kRoot) return std::uint8_t{0};  // Root is always at index 0
    if (const auto cached = featureIndexes_.find(featureId); cached != featureIndexes_.end()) return cached->second;

    // IRoot.getFeature(featureId) -> [4] index, 0 if the headset doesn't have it.
    const auto answer = CallIndex(0, featureId, kRootGetFeature,
                                  {static_cast<std::uint8_t>(featureId >> 8), static_cast<std::uint8_t>(featureId)});
    if (!answer) return std::unexpected(answer.error());
    const std::uint8_t index = (*answer)[4];
    if (index == 0) return std::unexpected(Error{.step = Step::NoFeature, .featureId = featureId});
    featureIndexes_[featureId] = index;
    return index;
}

// `featureId` only labels errors; `featureIndex` is what goes on the wire.
std::expected<Report, Error> Device::CallIndex(std::uint8_t featureIndex, std::uint16_t featureId,
                                               std::uint8_t function, const Params& params) {
    const auto functionAndSoftwareId = static_cast<std::uint8_t>((function << 4) | kSoftwareId);
    Report request{};  // `{}` zero-fills
    request[0] = kLongReportId;
    request[1] = kDeviceIndex;
    request[2] = featureIndex;
    request[3] = functionAndSoftwareId;
    std::ranges::copy(params, request.begin() + 4);

    if (const auto written = transport_.Write(request); !written) {
        return std::unexpected(ForFeature(written.error(), featureId));
    }

    const auto deadline = std::chrono::steady_clock::now() + kResponseTimeout;
    while (true) {
        const auto answer = transport_.Read(deadline);
        if (!answer) return std::unexpected(ForFeature(answer.error(), featureId));
        const Report& report = *answer;
        if (report[0] == kLongReportId) {
            if (report[2] == featureIndex && report[3] == functionAndSoftwareId) return report;
            if (report[2] == kErrorFeatureIndex && report[3] == featureIndex && report[4] == functionAndSoftwareId) {
                return std::unexpected(Error{.step = Step::HidppError, .hidppError = report[5], .featureId = featureId});
            }
        }
        // Anything else is unsolicited (an event, or the answer to another request): skip it. The deadline
        // check stops a stream of unrelated reports from keeping us here.
        if (std::chrono::steady_clock::now() >= deadline) {
            return std::unexpected(Error{.step = Step::Timeout, .featureId = featureId});
        }
    }
}

std::expected<ProtocolVersion, Error> GetProtocolVersion(Device& device) {
    constexpr std::uint8_t kPing = 0x5A;  // echoed back in [6]; not needed, but the function takes it
    const auto answer = device.Call(kRoot, kRootGetProtocolVersion, {0, 0, kPing});
    if (!answer) return std::unexpected(answer.error());
    return ProtocolVersion{.major = (*answer)[4], .minor = (*answer)[5]};
}

std::expected<std::string, Error> GetFirmwareVersion(Device& device) {
    const auto count = device.Call(kFirmwareInfo, kFirmwareGetEntityCount);
    if (!count) return std::unexpected(count.error());

    std::string text;
    for (std::uint8_t entity = 0; entity < (*count)[4]; ++entity) {
        const auto info = device.Call(kFirmwareInfo, kFirmwareGetInfo, {entity});
        if (!info) return std::unexpected(info.error());
        // [5..7] name prefix (ASCII, padded), [8] major and [9] minor (BCD), [10..11] build (BCD).
        const Report& report = *info;
        std::string prefix;
        for (std::size_t i = 5; i < 8; ++i) {
            if (report[i] > ' ' && report[i] < 0x7F) prefix += static_cast<char>(report[i]);
        }
        char version[32];
        std::snprintf(version, sizeof version, "%02X.%02X build %02X%02X", report[8], report[9], report[10], report[11]);
        if (!text.empty()) text += ", ";
        if (!prefix.empty()) text += prefix + " ";
        text += version;
    }
    return text;
}

std::expected<std::uint8_t, Error> GetSidetone(Device& device) {
    const auto answer = device.Call(kSidetone, kSidetoneGetLevel);
    if (!answer) return std::unexpected(answer.error());
    return (*answer)[4];
}

std::expected<void, Error> SetSidetone(Device& device, std::uint8_t level) {
    const auto answer = device.Call(kSidetone, kSidetoneSetLevel, {level});
    if (!answer) return std::unexpected(answer.error());
    return {};  // success, no value
}

}  // namespace hidpp
