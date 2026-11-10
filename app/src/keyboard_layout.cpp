#include "keyboard_layout.hpp"
#include <cstdlib>
#include <cstring>
#ifdef __SWITCH__
#include <switch.h>
#endif
namespace nxnow::input
{
KeyboardLayout SystemKeyboardLayout()
{
    return [] {
#ifdef __SWITCH__
        if (R_SUCCEEDED(setsysInitialize()))
        {
            SetKeyboardLayout layout = SetKeyboardLayout_EnglishUs;
            const auto result = setsysGetKeyboardLayout(&layout);
            setsysExit();
            if (R_SUCCEEDED(result))
            {
                switch (layout)
                {
                    case SetKeyboardLayout_Japanese: return KeyboardLayout::Japanese;
                    case SetKeyboardLayout_EnglishUs: return KeyboardLayout::Us;
                    case SetKeyboardLayout_EnglishUsInternational: return KeyboardLayout::UsInternational;
                    case SetKeyboardLayout_EnglishUk: return KeyboardLayout::Uk;
                    case SetKeyboardLayout_French: return KeyboardLayout::French;
                    case SetKeyboardLayout_FrenchCa: return KeyboardLayout::FrenchCanadian;
                    case SetKeyboardLayout_Spanish: return KeyboardLayout::Spanish;
                    case SetKeyboardLayout_SpanishLatin: return KeyboardLayout::LatinSpanish;
                    case SetKeyboardLayout_German: return KeyboardLayout::German;
                    case SetKeyboardLayout_Italian: return KeyboardLayout::Italian;
                    case SetKeyboardLayout_Portuguese: return KeyboardLayout::Portuguese;
                    case SetKeyboardLayout_Russian: return KeyboardLayout::Russian;
                    case SetKeyboardLayout_Korean: return KeyboardLayout::Korean;
                    case SetKeyboardLayout_ChineseSimplified: return KeyboardLayout::SimplifiedChinese;
                    case SetKeyboardLayout_ChineseTraditional: return KeyboardLayout::TraditionalChinese;
                }
            }
        }
        if (R_SUCCEEDED(setInitialize()))
        {
            u64 code = 0; const auto result = setGetSystemLanguage(&code); setExit();
            if (R_SUCCEEDED(result)) { char locale[9] {}; std::memcpy(locale, &code, sizeof(code)); return KeyboardLayoutForLocale(locale); }
        }
#else
        for (const char* variable : {"LC_ALL", "LC_MESSAGES", "LANG"})
            if (const char* locale = std::getenv(variable); locale && *locale) return KeyboardLayoutForLocale(locale);
#endif
        return KeyboardLayout::Us;
    }();
}
}
