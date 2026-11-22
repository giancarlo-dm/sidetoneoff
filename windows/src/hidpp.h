// HID++ 2.0, the protocol Logitech headsets speak over a vendor HID collection.
// Portable on purpose: no Windows headers, so a Linux or Mac port can reuse it (plan, section 5.0).
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <string>

namespace hidpp {

// The supported headset. Other models of the same family would be more rows here (plan, section 8).
inline constexpr std::uint16_t kLogitechVendorId = 0x046D;
inline constexpr std::uint16_t kG635ProductId = 0x0A89;

// The G635's vendor collection that carries HID++.
inline constexpr std::uint16_t kHidppUsagePage = 0xFF43;
inline constexpr std::uint16_t kHidppUsage = 0x0202;

// Every message here is a HID++ "long" report: 20 bytes, the first one being report ID 0x11.
inline constexpr std::size_t kReportSize = 20;
using Report = std::array<std::uint8_t, kReportSize>;  // a fixed-size byte array, like Java's byte[20]

// The parameter bytes of a request (bytes 4 to 19); the ones not given stay 0.
// At a call site, `{level}` fills the first byte, and more than 16 values don't compile.
using Params = std::array<std::uint8_t, kReportSize - 4>;

// The features this app uses (HID++ 2.0 feature IDs).
inline constexpr std::uint16_t kRoot = 0x0000;
inline constexpr std::uint16_t kFirmwareInfo = 0x0003;
inline constexpr std::uint16_t kSidetone = 0x8300;

// How long to wait for the answer to a request.
inline constexpr std::chrono::milliseconds kResponseTimeout{1000};

// The step that failed.
enum class Step : std::uint8_t {
    NotFound,    // the headset's HID++ collection isn't there
    Open,        // it is there but couldn't be opened
    Write,       // sending a request failed
    Read,        // receiving failed
    Timeout,     // no answer before the deadline
    HidppError,  // the headset answered with a HID++ error code
    NoFeature,   // the headset doesn't have the feature
    Verify,      // after a set, the headset reads back a different value
};

// What went wrong. Functions return it as a value, never throw it: see std::expected below.
struct Error {
    Step step = Step::NotFound;
    std::uint32_t osError = 0;    // OS error code (GetLastError on Windows), 0 if none
    std::uint8_t hidppError = 0;  // HID++ error code sent by the headset, 0 if none
    std::uint16_t featureId = 0;  // the feature being called, 0 if none
    std::uint8_t readBack = 0;    // Step::Verify: the value the headset reported
};

// One line for logs and --diag, e.g. "no answer from the headset within 1 s (feature 0x8300)".
std::string Describe(const Error& error);

// How reports reach the headset. Each platform implements it (WinHidTransport on Windows), and the unit
// tests use a fake. A class with only pure virtual functions (`= 0`) is C++'s version of an interface.
//
// The functions return std::expected<T, Error>: either the value or an Error, like a Result type
// (TS: `{ ok: true, value } | { ok: false, error }`). Check it with `if (!result)`, then read `*result`
// or `result.error()`.
class Transport {
public:
    virtual ~Transport() = default;

    // Sends one report.
    virtual std::expected<void, Error> Write(const Report& report) = 0;

    // Returns the next report from the headset, or Step::Timeout if none arrives before `deadline`.
    virtual std::expected<Report, Error> Read(std::chrono::steady_clock::time_point deadline) = 0;
};

// A HID++ 2.0 device reached through a Transport. It lives for one attempt.
class Device {
public:
    // `Transport&` is a reference: never null, not owned. The transport must outlive the Device.
    explicit Device(Transport& transport) : transport_(transport) {}

    // Calls `function` of feature `featureId` and returns the headset's answer. Reports that aren't
    // the answer are skipped. Fails with Step::Timeout if no answer arrives within kResponseTimeout.
    std::expected<Report, Error> Call(std::uint16_t featureId, std::uint8_t function, const Params& params = {});

    // The headset's index for `featureId`: asked once, then cached. Step::NoFeature if it lacks it.
    std::expected<std::uint8_t, Error> FeatureIndex(std::uint16_t featureId);

private:
    std::expected<Report, Error> CallIndex(std::uint8_t featureIndex, std::uint16_t featureId, std::uint8_t function,
                                           const Params& params);

    Transport& transport_;
    std::map<std::uint16_t, std::uint8_t> featureIndexes_;  // feature ID -> index
};

// Feature functions: one HID++ call each (two or more for the firmware). Free functions rather than
// methods, so a new feature (RGB, EQ) is a new function and Device doesn't change.

struct ProtocolVersion {
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
};
std::expected<ProtocolVersion, Error> GetProtocolVersion(Device& device);

// Firmware of each entity, e.g. "U1 30.00 build 0007", joined with ", " when there are several.
std::expected<std::string, Error> GetFirmwareVersion(Device& device);

// Sidetone level, 0-100 (feature 0x8300). The headset forgets it at power-off.
std::expected<std::uint8_t, Error> GetSidetone(Device& device);
std::expected<void, Error> SetSidetone(Device& device, std::uint8_t level);

}  // namespace hidpp
