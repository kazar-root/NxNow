#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace nxnow
{
inline std::string InterfaceLanguageForLocale(std::string locale)
{
    std::transform(locale.begin(), locale.end(), locale.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    const auto language = locale.substr(0, locale.find_first_of("_- .@"));
    if (language == "zh") return "zh-CN";
    for (const char* supported : {"fr", "es", "it", "ru", "uk", "pl"})
        if (language == supported) return language;
    return "en";
}
}
