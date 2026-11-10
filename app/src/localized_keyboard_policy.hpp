#pragma once
#include "physical_input_policy.hpp"
#include <string_view>
#include <vector>

namespace nxnow::input
{
struct TextKeySequence
{
    std::array<KeyboardStroke, 2> strokes {};
    size_t size = 0;
};

// French VK/character positions follow Microsoft's kbdfr layout. Physical
// strokes retain their scan code; text strokes also choose Shift/AltGr.
inline bool MapTextCharacter(char32_t c, KeyboardLayout layout, TextKeySequence& out)
{
    out = {};
    if (layout != KeyboardLayout::French)
    {
        // Never guess national punctuation (notably password symbols) or an
        // IME composition. USB physical input still uses the requested layout.
        if (layout == KeyboardLayout::Russian || layout == KeyboardLayout::Japanese ||
            layout == KeyboardLayout::Korean || layout == KeyboardLayout::SimplifiedChinese ||
            layout == KeyboardLayout::TraditionalChinese) return false;
        if (layout != KeyboardLayout::Us && layout != KeyboardLayout::UsInternational &&
            !((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') ||
              (c >= U'0' && c <= U'9') || c == U' ')) return false;
        if (c > 127 || !MapAsciiKey(static_cast<char>(c), out.strokes[0])) return false;
        out.size = 1;
        if (layout == KeyboardLayout::German)
        {
            if (c == U'y' || c == U'Y') out.strokes[0].scancode = 0x2c;
            if (c == U'z' || c == U'Z') out.strokes[0].scancode = 0x15;
        }
        return true;
    }
    unsigned usage = 0;
    uint16_t modifiers = 0;
    bool finish_dead = false;
    if ((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'))
    {
        const bool upper = c <= U'Z';
        const auto letter = upper ? c + 32 : c;
        usage = static_cast<unsigned>(letter - U'a' + 4);
        if (letter == U'a') usage = 20;
        if (letter == U'q') usage = 4;
        if (letter == U'w') usage = 29;
        if (letter == U'z') usage = 26;
        if (letter == U'm') usage = 51;
        modifiers = upper ? 1 : 0;
    }
    else if (c >= U'0' && c <= U'9')
    { usage = c == U'0' ? 39 : static_cast<unsigned>(c - U'1' + 30); modifiers = 1; }
    else
    {
        switch (c)
        {
            case U' ': usage = 44; break;
            case U'&': usage = 30; break; case U'é': usage = 31; break;
            case U'"': usage = 32; break; case U'\'': usage = 33; break;
            case U'(': usage = 34; break; case U'-': usage = 35; break;
            case U'è': usage = 36; break; case U'_': usage = 37; break;
            case U'ç': usage = 38; break; case U'à': usage = 39; break;
            case U')': usage = 45; break; case U'°': usage = 45; modifiers = 1; break;
            case U'=': usage = 46; break; case U'+': usage = 46; modifiers = 1; break;
            case U'$': usage = 48; break; case U'£': usage = 48; modifiers = 1; break;
            case U'*': usage = 49; break; case U'µ': usage = 49; modifiers = 1; break;
            case U'ù': usage = 52; break; case U'%': usage = 52; modifiers = 1; break;
            case U'²': usage = 53; break;
            case U',': usage = 16; break; case U'?': usage = 16; modifiers = 1; break;
            case U';': usage = 54; break; case U'.': usage = 54; modifiers = 1; break;
            case U':': usage = 55; break; case U'/': usage = 55; modifiers = 1; break;
            case U'!': usage = 56; break; case U'§': usage = 56; modifiers = 1; break;
            case U'<': usage = 100; break; case U'>': usage = 100; modifiers = 1; break;
            case U'@': usage = 39; modifiers = 6; break;
            case U'#': usage = 32; modifiers = 6; break;
            case U'{': usage = 33; modifiers = 6; break;
            case U'[': usage = 34; modifiers = 6; break;
            case U'|': usage = 35; modifiers = 6; break;
            case U'\\': usage = 37; modifiers = 6; break;
            case U'^': usage = 38; modifiers = 6; break;
            case U']': usage = 45; modifiers = 6; break;
            case U'}': usage = 46; modifiers = 6; break;
            case U'€': usage = 8; modifiers = 6; break;
            case U'~': usage = 31; modifiers = 6; finish_dead = true; break;
            case U'`': usage = 36; modifiers = 6; finish_dead = true; break;
            default:
            {
                const std::u32string_view accents = U"âêîôûäëïöüÿ";
                const auto index = accents.find(c);
                if (index == std::u32string_view::npos) return false;
                const char32_t base = std::u32string_view(U"aeiouaeiouy")[index];
                TextKeySequence letter;
                if (!MapTextCharacter(base, layout, letter)) return false;
                out.strokes[0] = PhysicalKeyStroke(47, layout);
                out.strokes[0].modifiers = index >= 5 ? 1 : 0;
                out.strokes[1] = letter.strokes[0]; out.size = 2; return true;
            }
        }
    }
    out.strokes[0] = PhysicalKeyStroke(usage, layout);
    out.strokes[0].modifiers = modifiers;
    out.size = 1;
    if (finish_dead) { out.strokes[1] = {0x20, 0x39, 0}; out.size = 2; }
    return out.strokes[0].keycode != 0;
}

inline bool DecodeRemoteText(std::string_view text, KeyboardLayout layout, std::vector<char32_t>& out)
{
    out.clear();
    for (size_t i = 0; i < text.size();)
    {
        unsigned char first = static_cast<unsigned char>(text[i++]);
        char32_t c = first; unsigned continuation = 0; char32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { c &= 31; continuation = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { c &= 15; continuation = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { c &= 7; continuation = 3; minimum = 0x10000; }
        else if (first >= 0x80) return false;
        for (unsigned j = 0; j < continuation; ++j)
        {
            if (i == text.size()) return false;
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) return false;
            c = (c << 6) | (next & 63);
        }
        TextKeySequence mapped;
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff) ||
            !MapTextCharacter(c, layout, mapped) || out.size() >= 500) return false;
        out.push_back(c);
    }
    return true;
}
inline bool IsLayoutTextSupported(std::string_view text, KeyboardLayout layout)
{ std::vector<char32_t> decoded; return DecodeRemoteText(text, layout, decoded); }
}
