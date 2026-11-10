#include "localized_keyboard_policy.hpp"
#include "deferred_keyboard_input.hpp"
#include <cassert>
#include <string>
#include <vector>
using namespace nxnow::input;
int main()
{
    assert(KeyboardLayoutForLocale("fr_FR.UTF-8") == KeyboardLayout::French);
    assert(KeyboardLayoutForLocale("fr-CA") == KeyboardLayout::FrenchCanadian);
    assert(KeyboardLayoutForLocale("en-GB") == KeyboardLayout::Uk);
    assert(KeyboardLayoutForLocale("de-DE") == KeyboardLayout::German);
    assert(std::string(CloudKeyboardLayout(KeyboardLayout::French)) == "fr-FR_azerty");
    assert(PhysicalKeyStroke(20, KeyboardLayout::French).keycode == 'A');
    assert(PhysicalKeyStroke(20, KeyboardLayout::French).scancode == 0x10);
    assert(PhysicalKeyStroke(4, KeyboardLayout::French).keycode == 'Q');
    assert(PhysicalKeyStroke(26, KeyboardLayout::French).keycode == 'Z');
    assert(PhysicalKeyStroke(29, KeyboardLayout::French).keycode == 'W');
    assert(PhysicalKeyStroke(51, KeyboardLayout::French).keycode == 'M');
    assert(PhysicalKeyStroke(16, KeyboardLayout::French).keycode == 0xbc);
    TextKeySequence text;
    assert(MapTextCharacter(U'@', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == '0' && text.strokes[0].modifiers == 6);
    assert(MapTextCharacter(U'1', KeyboardLayout::French, text) && text.strokes[0].modifiers == 1);
    assert(MapTextCharacter(U'é', KeyboardLayout::French, text) && text.strokes[0].keycode == '2');
    assert(MapTextCharacter(U'ê', KeyboardLayout::French, text) && text.size == 2);
    assert(MapTextCharacter(U'~', KeyboardLayout::French, text) && text.size == 2);
    for (char32_t c = 32; c < 127; ++c) assert(MapTextCharacter(c, KeyboardLayout::French, text));
    assert(IsLayoutTextSupported("été à Noël, ça marche ! azerty AZERTY 123 @#€", KeyboardLayout::French));
    assert(!IsLayoutTextSupported("\xc0\xaf", KeyboardLayout::French));
    assert(!IsLayoutTextSupported("\xed\xa0\x80", KeyboardLayout::French));
    assert(!IsLayoutTextSupported("\xf4\x90\x80\x80", KeyboardLayout::French));
    DeferredKeyboardInput pending;
    assert(pending.AppendEdit("é", "è", KeyboardLayout::French));
    std::vector<KeyboardEvent> sent;
    pending.Drain(true, [&](const auto& event) { sent.push_back(event); return true; });
    assert(sent.size() == 4); // One backspace, one accented character (not UTF-8 bytes).
    assert(sent[0].stroke.keycode == 8 && sent[2].stroke.keycode == '7');
    assert(pending.AppendText("à bientôt !", KeyboardLayout::French));
    const auto size = pending.size(); pending.Drain(false, [](auto) { assert(false); return true; });
    assert(pending.size() == size);
    pending.Clear(); std::string repeated; for (int i = 0; i < 500; ++i) repeated += "é";
    assert(pending.AppendText(repeated, KeyboardLayout::French));
}
