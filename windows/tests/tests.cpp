// Unit tests for the portable code (hidpp, profile, log_history), run by build.ps1 on every build. A
// FakeTransport stands in for the headset, so no hardware is needed. No framework: each test is a function,
// CHECK records failures.

#include <cstdio>
#include <deque>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "hidpp.h"
#include "log_history.h"
#include "profile.h"

namespace {

using hidpp::Error;
using hidpp::Report;
using hidpp::Step;

int failures = 0;

void Check(bool ok, const char* expression, int line) {
    if (ok) return;
    ++failures;
    std::printf("  tests.cpp(%d): CHECK failed: %s\n", line, expression);
}

// A macro, so the failure message can show the expression's source text and line.
#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

// A report from its first bytes; the rest are 0.
Report Bytes(std::initializer_list<std::uint8_t> bytes) {
    Report report{};
    std::size_t i = 0;
    for (std::uint8_t byte : bytes) report[i++] = byte;
    return report;
}

// Plays the headset: records what was written, and answers each Read with the next queued report.
// An empty queue means the headset stays silent, i.e. a timeout.
class FakeTransport final : public hidpp::Transport {
public:
    std::vector<Report> written;
    std::deque<Report> answers;
    std::optional<Error> writeError;  // when set, every Write fails with it

    std::expected<void, Error> Write(const Report& report) override {
        if (writeError) return std::unexpected(*writeError);
        written.push_back(report);
        return {};
    }

    std::expected<Report, Error> Read(std::chrono::steady_clock::time_point) override {
        if (answers.empty()) return std::unexpected(Error{.step = Step::Timeout});
        const Report next = answers.front();
        answers.pop_front();
        return next;
    }
};

// IRoot.getFeature answer: the sidetone feature is at index 7, as on the real G635.
const Report kSidetoneAt7 = Bytes({0x11, 0xFF, 0x00, 0x0A, 0x07});

void SetSidetoneSendsTheSpecPacket() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0x07, 0x1A})};
    hidpp::Device device{transport};

    CHECK(hidpp::SetSidetone(device, 0));
    CHECK(transport.written.size() == 2);
    CHECK(transport.written.size() == 2 && transport.written[0] == Bytes({0x11, 0xFF, 0x00, 0x0A, 0x83, 0x00}));
    CHECK(transport.written.size() == 2 && transport.written[1] == Bytes({0x11, 0xFF, 0x07, 0x1A, 0x00}));
}

void GetSidetoneReadsTheLevel() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0x07, 0x0A, 40})};
    hidpp::Device device{transport};

    const auto level = hidpp::GetSidetone(device);
    CHECK(level && *level == 40);
    CHECK(transport.written.size() == 2 && transport.written[1] == Bytes({0x11, 0xFF, 0x07, 0x0A}));
}

void FeatureIndexIsAskedOnce() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0x07, 0x1A}), Bytes({0x11, 0xFF, 0x07, 0x0A, 0})};
    hidpp::Device device{transport};

    CHECK(hidpp::SetSidetone(device, 0));
    CHECK(hidpp::GetSidetone(device));
    CHECK(transport.written.size() == 3);  // one getFeature, one set, one get
}

void UnsolicitedReportsAreSkipped() {
    FakeTransport transport;
    transport.answers = {
        kSidetoneAt7,
        Bytes({0x11, 0xFF, 0x04, 0x00, 0x01}),        // an event from another feature
        Bytes({0x11, 0xFF, 0x07, 0x1B, 0x00}),        // same function, another software ID
        Bytes({0x11, 0xFF, 0xFF, 0x07, 0x1B, 0x02}),  // an error for someone else's request
        Bytes({0x10, 0xFF, 0x07, 0x0A, 0x63}),        // a short report
        Bytes({0x11, 0xFF, 0x07, 0x0A, 0x00}),        // the answer
    };
    hidpp::Device device{transport};

    const auto level = hidpp::GetSidetone(device);
    CHECK(level && *level == 0);
}

void HidppErrorIsReported() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0xFF, 0x07, 0x1A, 0x03})};
    hidpp::Device device{transport};

    const auto result = hidpp::SetSidetone(device, 200);
    CHECK(!result && result.error().step == Step::HidppError);
    CHECK(!result && result.error().hidppError == 0x03);
    CHECK(!result && result.error().featureId == hidpp::kSidetone);
}

void MissingFeatureFails() {
    FakeTransport transport;
    transport.answers = {Bytes({0x11, 0xFF, 0x00, 0x0A, 0x00})};  // getFeature: index 0 = not there
    hidpp::Device device{transport};

    const auto result = hidpp::SetSidetone(device, 0);
    CHECK(!result && result.error().step == Step::NoFeature);
    CHECK(!result && result.error().featureId == hidpp::kSidetone);
    CHECK(transport.written.size() == 1);  // the set was never sent
}

void SilenceTimesOut() {
    FakeTransport transport;
    hidpp::Device device{transport};

    const auto result = hidpp::GetSidetone(device);
    CHECK(!result && result.error().step == Step::Timeout);
    CHECK(!result && result.error().featureId == hidpp::kSidetone);
}

void WriteErrorIsPassedOn() {
    FakeTransport transport;
    transport.writeError = Error{.step = Step::Write, .osError = 1167};  // ERROR_DEVICE_NOT_CONNECTED
    hidpp::Device device{transport};

    const auto result = hidpp::SetSidetone(device, 0);
    CHECK(!result && result.error().step == Step::Write);
    CHECK(!result && result.error().osError == 1167);
    CHECK(!result && result.error().featureId == hidpp::kSidetone);
}

void RootNeedsNoLookup() {
    FakeTransport transport;
    transport.answers = {Bytes({0x11, 0xFF, 0x00, 0x1A, 4, 2, 0x5A})};
    hidpp::Device device{transport};

    const auto version = hidpp::GetProtocolVersion(device);
    CHECK(version && version->major == 4 && version->minor == 2);
    CHECK(transport.written.size() == 1 && transport.written[0] == Bytes({0x11, 0xFF, 0x00, 0x1A, 0, 0, 0x5A}));
}

void FirmwareVersionIsFormatted() {
    FakeTransport transport;
    transport.answers = {
        Bytes({0x11, 0xFF, 0x00, 0x0A, 0x02}),                                // getFeature(0x0003): index 2
        Bytes({0x11, 0xFF, 0x02, 0x0A, 1}),                                   // one entity
        Bytes({0x11, 0xFF, 0x02, 0x1A, 0, 'U', '1', ' ', 0x30, 0x00, 0x00, 0x07}),  // as the real G635
    };
    hidpp::Device device{transport};

    const auto firmware = hidpp::GetFirmwareVersion(device);
    CHECK(firmware && *firmware == "U1 30.00 build 0007");
}

void ErrorsDescribeThemselves() {
    CHECK(hidpp::Describe(Error{.step = Step::NotFound}) == "G635 not found");
    CHECK(hidpp::Describe(Error{.step = Step::HidppError, .hidppError = 3, .featureId = 0x8300}) ==
          "the headset returned HID++ error 0x03 OutOfRange (feature 0x8300)");
    CHECK(hidpp::Describe(Error{.step = Step::Write, .osError = 1167, .featureId = 0x8300}) ==
          "couldn't send to the headset (feature 0x8300) (OS error 1167)");
    CHECK(hidpp::Describe(Error{.step = Step::Verify, .featureId = 0x8300, .readBack = 40}) ==
          "the headset reads back 40 (feature 0x8300)");
}

// --- profile ---------------------------------------------------------------------------------------

void ApplySetsThenReadsBack() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0x07, 0x1A}), Bytes({0x11, 0xFF, 0x07, 0x0A, 0})};
    hidpp::Device device{transport};

    CHECK(Apply(device, Profile{.sidetone = std::uint8_t{0}}));
    CHECK(transport.written.size() == 3);  // getFeature, set 0, get
    CHECK(transport.written.size() == 3 && transport.written[1] == Bytes({0x11, 0xFF, 0x07, 0x1A, 0x00}));
    CHECK(transport.written.size() == 3 && transport.written[2] == Bytes({0x11, 0xFF, 0x07, 0x0A}));
}

void ApplyFailsWhenTheReadBackDiffers() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0x07, 0x1A}), Bytes({0x11, 0xFF, 0x07, 0x0A, 40})};
    hidpp::Device device{transport};

    const auto result = Apply(device, Profile{.sidetone = std::uint8_t{0}});
    CHECK(!result && result.error().step == Step::Verify);
    CHECK(!result && result.error().readBack == 40);
    CHECK(!result && result.error().featureId == hidpp::kSidetone);
}

void ApplyFailsWhenTheReadBackTimesOut() {  // decision M2
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0x07, 0x1A})};  // the get gets no answer
    hidpp::Device device{transport};

    const auto result = Apply(device, Profile{.sidetone = std::uint8_t{0}});
    CHECK(!result && result.error().step == Step::Timeout);
}

void ApplyStopsWhenTheSetFails() {
    FakeTransport transport;
    transport.answers = {kSidetoneAt7, Bytes({0x11, 0xFF, 0xFF, 0x07, 0x1A, 0x04})};  // HardwareError
    hidpp::Device device{transport};

    const auto result = Apply(device, Profile{.sidetone = std::uint8_t{0}});
    CHECK(!result && result.error().step == Step::HidppError);
    CHECK(transport.written.size() == 2);  // no get after the failed set
}

void EmptyProfileSendsNothing() {
    FakeTransport transport;
    hidpp::Device device{transport};

    CHECK(Apply(device, Profile{.sidetone = std::nullopt}));
    CHECK(transport.written.empty());
}

void LogHistoryReadsEverythingFromTheStart() {
    LogHistory history(10);
    history.Add(L"one");
    history.Add(L"two");
    std::uint64_t cursor = 0;
    CHECK(history.TextSince(cursor) == L"one\r\ntwo\r\n");
    CHECK(cursor == 2);
}

void LogHistoryReadsOnlyNewLines() {
    LogHistory history(10);
    history.Add(L"one");
    std::uint64_t cursor = 0;
    history.TextSince(cursor);
    history.Add(L"two");
    history.Add(L"three");
    CHECK(history.TextSince(cursor) == L"two\r\nthree\r\n");
    CHECK(history.TextSince(cursor).empty());  // nothing new since
}

void LogHistoryKeepsTheNewestLines() {
    LogHistory history(3);
    for (const wchar_t* line : {L"1", L"2", L"3", L"4", L"5"}) history.Add(line);
    std::uint64_t cursor = 0;
    CHECK(history.TextSince(cursor) == L"3\r\n4\r\n5\r\n");
    CHECK(cursor == 5);
}

void LogHistorySkipsLinesDroppedSinceTheLastRead() {
    LogHistory history(2);
    history.Add(L"a");
    std::uint64_t cursor = 0;
    history.TextSince(cursor);  // read "a"
    history.Add(L"b");          // dropped below, before anyone reads it
    history.Add(L"c");
    history.Add(L"d");
    CHECK(history.TextSince(cursor) == L"c\r\nd\r\n");
}

struct Test {
    const char* name;
    void (*run)();  // a pointer to a function taking nothing and returning nothing
};

const Test kTests[] = {
    {"SetSidetoneSendsTheSpecPacket", SetSidetoneSendsTheSpecPacket},
    {"GetSidetoneReadsTheLevel", GetSidetoneReadsTheLevel},
    {"FeatureIndexIsAskedOnce", FeatureIndexIsAskedOnce},
    {"UnsolicitedReportsAreSkipped", UnsolicitedReportsAreSkipped},
    {"HidppErrorIsReported", HidppErrorIsReported},
    {"MissingFeatureFails", MissingFeatureFails},
    {"SilenceTimesOut", SilenceTimesOut},
    {"WriteErrorIsPassedOn", WriteErrorIsPassedOn},
    {"RootNeedsNoLookup", RootNeedsNoLookup},
    {"FirmwareVersionIsFormatted", FirmwareVersionIsFormatted},
    {"ErrorsDescribeThemselves", ErrorsDescribeThemselves},
    {"ApplySetsThenReadsBack", ApplySetsThenReadsBack},
    {"ApplyFailsWhenTheReadBackDiffers", ApplyFailsWhenTheReadBackDiffers},
    {"ApplyFailsWhenTheReadBackTimesOut", ApplyFailsWhenTheReadBackTimesOut},
    {"ApplyStopsWhenTheSetFails", ApplyStopsWhenTheSetFails},
    {"EmptyProfileSendsNothing", EmptyProfileSendsNothing},
    {"LogHistoryReadsEverythingFromTheStart", LogHistoryReadsEverythingFromTheStart},
    {"LogHistoryReadsOnlyNewLines", LogHistoryReadsOnlyNewLines},
    {"LogHistoryKeepsTheNewestLines", LogHistoryKeepsTheNewestLines},
    {"LogHistorySkipsLinesDroppedSinceTheLastRead", LogHistorySkipsLinesDroppedSinceTheLastRead},
};

}  // namespace

int main() {
    int failedTests = 0;
    for (const Test& test : kTests) {
        const int failuresBefore = failures;
        test.run();
        if (failures != failuresBefore) {
            ++failedTests;
            std::printf("FAIL %s\n", test.name);
        }
    }
    const int total = static_cast<int>(std::size(kTests));
    std::printf("%d of %d tests passed\n", total - failedTests, total);
    return failedTests == 0 ? 0 : 1;
}
