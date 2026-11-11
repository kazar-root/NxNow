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
    assert(std::string(CloudKeyboardLayout(KeyboardLayout::French)) == "fr-FR");
    assert(std::string(CloudKeyboardLayout(KeyboardLayout::Spanish)) == "es-ES_tradnl");
    assert(std::string(CloudKeyboardLayout(KeyboardLayout::FrenchCanadian)) == "en-US");
    assert(KeyboardLayoutForCloudCode("fr-FR") == KeyboardLayout::French);
    assert(KeyboardLayoutForCloudCode("fr-FR_azerty") == KeyboardLayout::French);
    assert(KeyboardLayoutForCloudCode("de-DE_qwertz") == KeyboardLayout::German);
    assert(KeyboardLayoutForCloudCode("fr-CA_qwerty") == KeyboardLayout::FrenchCanadian);
    assert(KeyboardLayoutForCloudCode("es-ES_tradnl") == KeyboardLayout::Spanish);
    assert(KeyboardLayoutForCloudCode("ja-106") == KeyboardLayout::Japanese);
    assert(KeyboardLayoutForCloudCode("en-US") == KeyboardLayout::Us);
    // Strokes describe positions: the AZERTY "A" key is the US "Q" position.
    assert(PhysicalKeyStroke(20, KeyboardLayout::French).keycode == 'Q');
    assert(PhysicalKeyStroke(20, KeyboardLayout::French).scancode == 0x10);
    assert(PhysicalKeyStroke(4, KeyboardLayout::French).keycode == 'A');
    assert(PhysicalKeyStroke(4, KeyboardLayout::French).scancode == 0x1e);
    assert(PhysicalKeyStroke(51, KeyboardLayout::French).keycode == 0xba);
    assert(PhysicalKeyStroke(16, KeyboardLayout::French).keycode == 'M');
    TextKeySequence text;
    // Typing "a" presses the AZERTY A key (US Q position), "q" the US A position.
    assert(MapTextCharacter(U'a', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == 'Q' && text.strokes[0].scancode == 0x10 && text.strokes[0].modifiers == 0);
    assert(MapTextCharacter(U'Q', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == 'A' && text.strokes[0].scancode == 0x1e && text.strokes[0].modifiers == 1);
    assert(MapTextCharacter(U'w', KeyboardLayout::French, text) && text.strokes[0].keycode == 'Z');
    assert(MapTextCharacter(U'm', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == 0xba && text.strokes[0].scancode == 0x27);
    assert(MapTextCharacter(U',', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == 'M' && text.strokes[0].scancode == 0x32);
    assert(MapTextCharacter(U'0', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == '0' && text.strokes[0].scancode == 0x0b && text.strokes[0].modifiers == 1);
    assert(MapTextCharacter(U'(', KeyboardLayout::French, text) && text.strokes[0].keycode == '5');
    assert(MapTextCharacter(U']', KeyboardLayout::French, text));
    assert(text.strokes[0].keycode == 0xbd && text.strokes[0].modifiers == 6);
    assert(MapTextCharacter(U'y', KeyboardLayout::German, text) && text.strokes[0].keycode == 'Z');
    assert(MapTextCharacter(U'Z', KeyboardLayout::German, text));
    assert(text.strokes[0].keycode == 'Y' && text.strokes[0].modifiers == 1);
    assert(MapTextCharacter(U'q', KeyboardLayout::Us, text) && text.strokes[0].keycode == 'Q');
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
