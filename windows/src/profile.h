// What the app applies to the headset on every connection (plan, section 5.0). Portable, like hidpp.
#pragma once

#include <cstdint>
#include <expected>
#include <optional>

#include "hidpp.h"

struct Profile {
    // The sidetone level to set; std::nullopt (empty) leaves the headset's level alone.
    std::optional<std::uint8_t> sidetone = std::uint8_t{0};
    // Next version (M7): std::optional<LedSettings> leds;
};

// Sets everything in `profile`, then reads each value back. Succeeds only if the headset confirms all of
// them: a read-back that differs, or doesn't come, is a failure (decision M2).
std::expected<void, hidpp::Error> Apply(hidpp::Device& device, const Profile& profile);
