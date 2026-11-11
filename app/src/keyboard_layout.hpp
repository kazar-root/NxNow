#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace nxnow::input
{
enum class KeyboardLayout { Us, UsInternational, Uk, French, FrenchCanadian, German, Spanish, LatinSpanish, Italian, Portuguese, Russian, Japanese, Korean, SimplifiedChinese, TraditionalChinese };
inline KeyboardLayout KeyboardLayoutForLocale(std::string locale)
{
    std::transform(locale.begin(), locale.end(), locale.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(locale.begin(), locale.end(), '_', '-');
    if (locale.starts_with("fr-ca")) return KeyboardLayout::FrenchCanadian;
    if (locale.starts_with("fr")) return KeyboardLayout::French;
    if (locale.starts_with("en-gb")) return KeyboardLayout::Uk;
    if (locale.starts_with("de")) return KeyboardLayout::German;
    if (locale.starts_with("es-419") || locale.starts_with("es-mx")) return KeyboardLayout::LatinSpanish;
    if (locale.starts_with("es")) return KeyboardLayout::Spanish;
    if (locale.starts_with("it")) return KeyboardLayout::Italian;
    if (locale.starts_with("pt")) return KeyboardLayout::Portuguese;
    if (locale.starts_with("ru")) return KeyboardLayout::Russian;
    if (locale.starts_with("ja")) return KeyboardLayout::Japanese;
    if (locale.starts_with("ko")) return KeyboardLayout::Korean;
    if (locale.starts_with("zh-tw") || locale.starts_with("zh-hant")) return KeyboardLayout::TraditionalChinese;
    if (locale.starts_with("zh")) return KeyboardLayout::SimplifiedChinese;
    return KeyboardLayout::Us;
}
// GeForce NOW only accepts its own layout identifiers (the ones its official
// clients send). An unknown value such as "fr-FR_azerty" is silently replaced
// by en-US on the cloud PC, so AZERTY-mapped text then types wrong symbols.
inline const char* CloudKeyboardLayout(KeyboardLayout layout)
{
    switch (layout)
    {
        case KeyboardLayout::French: return "fr-FR";
        case KeyboardLayout::Uk: return "en-GB";
        case KeyboardLayout::German: return "de-DE";
        case KeyboardLayout::Spanish: return "es-ES_tradnl";
        case KeyboardLayout::LatinSpanish: return "es-MX";
        case KeyboardLayout::Italian: return "it-IT";
        case KeyboardLayout::Portuguese: return "pt-PT";
        case KeyboardLayout::Russian: return "ru-RU";
        case KeyboardLayout::Japanese: return "ja-106";
        case KeyboardLayout::Korean: return "ko-KR";
        case KeyboardLayout::SimplifiedChinese: return "zh-CN";
        case KeyboardLayout::TraditionalChinese: return "zh-TW";
        // GeForce NOW has no Canadian French layout; text for it is mapped
        // with US key positions, so request the matching US layout.
        default: return "en-US";
    }
}
inline KeyboardLayout KeyboardLayoutForCloudCode(std::string_view code)
{
    // Older builds saved/sent "<locale>_<azerty|qwerty|qwertz>" identifiers.
    if (const auto underscore = code.find('_');
        underscore != std::string_view::npos && code.substr(underscore) != "_tradnl")
    {
        const auto suffix = code.substr(underscore + 1);
        if (suffix == "azerty" || suffix == "qwerty" || suffix == "qwertz")
        {
            if (code.substr(0, underscore) == "fr-CA") return KeyboardLayout::FrenchCanadian;
            code = code.substr(0, underscore);
        }
    }
    if (code == "es-ES") return KeyboardLayout::Spanish;
    if (code == "ja-JP") return KeyboardLayout::Japanese;
    for (auto layout : {KeyboardLayout::French, KeyboardLayout::Uk,
         KeyboardLayout::German, KeyboardLayout::Spanish, KeyboardLayout::LatinSpanish,
         KeyboardLayout::Italian, KeyboardLayout::Portuguese, KeyboardLayout::Russian,
         KeyboardLayout::Japanese, KeyboardLayout::Korean, KeyboardLayout::SimplifiedChinese,
         KeyboardLayout::TraditionalChinese})
        if (code == CloudKeyboardLayout(layout)) return layout;
    return KeyboardLayout::Us;
}
KeyboardLayout SystemKeyboardLayout();
}
