#include "profile.h"

std::expected<void, hidpp::Error> Apply(hidpp::Device& device, const Profile& profile) {
    if (profile.sidetone) {  // an optional tests true when it holds a value; `*` reads it
        const auto set = hidpp::SetSidetone(device, *profile.sidetone);
        if (!set) return set;
        const auto level = hidpp::GetSidetone(device);
        if (!level) return std::unexpected(level.error());
        if (*level != *profile.sidetone) {
            return std::unexpected(
                hidpp::Error{.step = hidpp::Step::Verify, .featureId = hidpp::kSidetone, .readBack = *level});
        }
    }
    return {};
}
