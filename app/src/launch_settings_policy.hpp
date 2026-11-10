#pragma once

#include "stream_settings.hpp"
#include "localization.hpp"
#include "keyboard_layout.hpp"

#include <string>

namespace nxnow::settings
{

inline std::string InterfaceGameLocale(const std::string& language)
{
    if (language == "fr") return "fr_FR";
    if (language == "de") return "de_DE";
    if (language == "es") return "es_ES";
    if (language == "it") return "it_IT";
    if (language == "ru") return "ru_RU";
    if (language == "uk") return "uk_UA";
    if (language == "pl") return "pl_PL";
    if (language == "zh-CN") return "zh_CN";
    return "en_US";
}

inline StreamSettings ResolveLaunchSettings(StreamSettings settings)
{
    if (settings.interface_language == "auto")
        settings.interface_language = SystemInterfaceLanguage();
    if (settings.keyboard_layout == "auto")
        settings.keyboard_layout = input::CloudKeyboardLayout(input::SystemKeyboardLayout());
    if (settings.game_language == "Auto")
        settings.game_language = InterfaceGameLocale(settings.interface_language);
    return settings;
}

} // namespace nxnow::settings
