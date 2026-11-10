#include "keyboard_input_policy.hpp"

#include <cassert>

int main()
{
    using nxnow::input::KeyboardStroke;
    using nxnow::input::KeyboardShortcut;
    using nxnow::input::MapAsciiKey;
    using nxnow::input::MapKeyboardShortcut;

    KeyboardStroke stroke;
    assert(MapAsciiKey('a', stroke));
    assert(stroke.keycode == 'A' && stroke.scancode == 0x1e && stroke.modifiers == 0);

    assert(MapAsciiKey('A', stroke));
    assert(stroke.keycode == 'A' && stroke.scancode == 0x1e && stroke.modifiers == 0x0001);

    assert(MapAsciiKey('@', stroke));
    assert(stroke.keycode == '2' && stroke.scancode == 0x03 && stroke.modifiers == 0x0001);

    assert(MapAsciiKey('?', stroke));
    assert(stroke.keycode == 0xbf && stroke.scancode == 0x35 && stroke.modifiers == 0x0001);

    assert(MapAsciiKey(' ', stroke));
    assert(stroke.keycode == 0x20 && stroke.scancode == 0x39 && stroke.modifiers == 0);

    assert(!MapAsciiKey('\t', stroke));

    // Every printable ASCII character, including all common password symbols,
    // must map to an actual remote key. Unsupported text must be rejected whole
    // so a password cannot be silently sent with missing characters.
    for (int character = 0x20; character <= 0x7e; ++character)
        assert(MapAsciiKey(static_cast<char>(character), stroke));
    assert(nxnow::input::IsRemoteKeyboardTextSupported(""));
    assert(nxnow::input::IsRemoteKeyboardTextSupported("player+switch@example.test"));
    assert(nxnow::input::IsRemoteKeyboardTextSupported("aA0!@#$%^&*()_+-=[]{};:'\"\\|,.<>/?`~ "));
    assert(!nxnow::input::IsRemoteKeyboardTextSupported("caf\xc3\xa9"));
    assert(!nxnow::input::IsRemoteKeyboardTextSupported("abc\ndef"));
    assert(!nxnow::input::IsRemoteKeyboardTextSupported("abc\tdef"));

    stroke = MapKeyboardShortcut(KeyboardShortcut::Escape);
    assert(stroke.keycode == 0x1b && stroke.scancode == 0x01 && stroke.modifiers == 0);

    stroke = MapKeyboardShortcut(KeyboardShortcut::Tab);
    assert(stroke.keycode == 0x09 && stroke.scancode == 0x0f && stroke.modifiers == 0);

    stroke = MapKeyboardShortcut(KeyboardShortcut::AltTab);
    assert(stroke.keycode == 0x09 && stroke.scancode == 0x0f &&
           stroke.modifiers == 0x0004);

    stroke = MapKeyboardShortcut(KeyboardShortcut::Windows);
    assert(stroke.keycode == 0x5b && stroke.scancode == 0x5b &&
           stroke.modifiers == 0x0008);
    return 0;
}
