#pragma once

#include <string>
#include <string_view>

namespace nxnow::input
{
inline std::string NormalizeGameInputName(std::string_view value)
{
    std::string normalized;
    for (unsigned char c : value)
    {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            normalized += static_cast<char>(c);
    }
    return normalized;
}

inline bool IsBattleNetStore(std::string_view store)
{
    const auto name = NormalizeGameInputName(store);
    return name == "bnet" || name == "battlenet" || name == "blizzard" ||
        name == "blizzardbattlenet";
}

inline bool IsWorldOfWarcraft(std::string_view title)
{
    const auto name = NormalizeGameInputName(title);
    return name.starts_with("worldofwarcraft") || name == "wow" ||
        name == "wowclassic" || name == "wowretail";
}

inline std::string ControllerLayoutForGame(std::string_view title, std::string_view selected)
{
    // WoW uses the requested positional mapping: A/B and X/Y cross once at
    // the Switch-to-XInput boundary. Other games retain the user's choice.
    return IsWorldOfWarcraft(title) ? "Xbox" : std::string(selected);
}
} // namespace nxnow::input
