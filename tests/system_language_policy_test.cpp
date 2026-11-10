#include "system_language_policy.hpp"
#include "localization.hpp"
#include "launch_settings_policy.hpp"
#include <cassert>
#include <cstdlib>
#include <utility>

int main()
{
    for (const auto& [locale, language] : {
        std::pair{"fr", "fr"}, {"fr-CA", "fr"}, {"FR_fr.UTF-8", "fr"},
        {"en-US", "en"}, {"en-GB", "en"}, {"es-419", "es"}, {"it-IT", "it"},
        {"ru-RU", "ru"}, {"pl_PL", "pl"}, {"uk_UA", "uk"},
        {"zh-Hans", "zh-CN"}, {"zh-Hant", "zh-CN"}, {"zh_TW", "zh-CN"},
        {"ja-JP", "en"}, {"de-DE", "en"}, {"pt-BR", "en"}, {"C.UTF-8", "en"}, {"", "en"}})
        assert(nxnow::InterfaceLanguageForLocale(locale) == language);
    setenv("LC_ALL", "fr_CA.UTF-8", 1);
    nxnow::SetInterfaceLanguage("auto");
    assert(nxnow::GetInterfaceLanguage() == "fr");
    nxnow::StreamSettings saved;
    const auto launched = nxnow::settings::ResolveLaunchSettings(saved);
    assert(saved.interface_language == "auto" && saved.game_language == "Auto");
    assert(launched.interface_language == "fr" && launched.game_language == "fr_FR");
    saved.game_language = "en_GB";
    assert(nxnow::settings::ResolveLaunchSettings(saved).game_language == "en_GB");
    nxnow::SetInterfaceLanguage("en");
    assert(nxnow::GetInterfaceLanguage() == "en");
    nxnow::SetInterfaceLanguage("auto");
    assert(nxnow::GetInterfaceLanguage() == "fr");
}
