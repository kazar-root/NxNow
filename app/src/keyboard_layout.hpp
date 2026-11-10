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
inline const char* CloudKeyboardLayout(KeyboardLayout layout)
{
    switch (layout)
    {
        case KeyboardLayout::French: return "fr-FR_azerty";
        case KeyboardLayout::FrenchCanadian: return "fr-CA_qwerty";
        case KeyboardLayout::Uk: return "en-GB_qwerty";
        case KeyboardLayout::German: return "de-DE_qwertz";
        case KeyboardLayout::Spanish: return "es-ES_qwerty";
        case KeyboardLayout::LatinSpanish: return "es-MX_qwerty";
        case KeyboardLayout::Italian: return "it-IT_qwerty";
        case KeyboardLayout::Portuguese: return "pt-PT_qwerty";
        case KeyboardLayout::Russian: return "ru-RU_qwerty";
        case KeyboardLayout::Japanese: return "ja-JP_qwerty";
        case KeyboardLayout::Korean: return "ko-KR_qwerty";
        case KeyboardLayout::SimplifiedChinese: return "zh-CN_qwerty";
        case KeyboardLayout::TraditionalChinese: return "zh-TW_qwerty";
        default: return "en-US_qwerty";
    }
}
inline KeyboardLayout KeyboardLayoutForCloudCode(std::string_view code)
{
    for (auto layout : {KeyboardLayout::French, KeyboardLayout::FrenchCanadian, KeyboardLayout::Uk,
         KeyboardLayout::German, KeyboardLayout::Spanish, KeyboardLayout::LatinSpanish,
         KeyboardLayout::Italian, KeyboardLayout::Portuguese, KeyboardLayout::Russian,
         KeyboardLayout::Japanese, KeyboardLayout::Korean, KeyboardLayout::SimplifiedChinese,
         KeyboardLayout::TraditionalChinese})
        if (code == CloudKeyboardLayout(layout)) return layout;
    return KeyboardLayout::Us;
}
KeyboardLayout SystemKeyboardLayout();
}
