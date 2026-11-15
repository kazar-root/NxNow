#include "wow_chat_chord.hpp"
#include <cassert>
#include <initializer_list>

int main()
{
    using namespace nxnow::input;
    assert(UseWowChatKeyboard(true, false));   // in-game chat in gamepad mode
    assert(!UseWowChatKeyboard(true, true));   // sign-in fields in mouse mode
    assert(!UseWowChatKeyboard(false, false)); // other games: plain keyboard
    assert(!UseWowChatKeyboard(false, true));

    {
        KeyboardCloseChord close;
        assert(!close.Update(true, true, false));   // chord that opens the keyboard
        assert(!close.Update(true, true, true));    // still held as it appears
        assert(!close.Update(false, false, true));
        assert(!close.Update(true, false, true));   // R3 alone does not close
        assert(!close.Update(false, false, true));
        assert(!close.Update(true, true, true));
        assert(!close.Update(false, true, true));   // wait for both releases
        assert(close.Update(false, false, true));   // closes
        assert(!close.Update(false, false, true));
        assert(!close.Update(true, true, true));
        assert(!close.Update(false, false, false)); // closed some other way
    }
    for (bool same_frame : {false, true})
    {
        for (bool release_r3_first : {false, true})
        {
            WowChatChord chord;
            auto d = chord.Update(true, same_frame, true);
            assert(d.capture && !d.open && !d.tap_r3);
            d = chord.Update(true, true, true);
            assert(d.capture && !d.open);
            for (int frame = 0; frame < 100; ++frame)
                assert(chord.Update(true, true, true).capture);
            d = chord.Update(!release_r3_first, release_r3_first, true);
            assert(d.capture && !d.open && !d.tap_r3);
            d = chord.Update(false, false, true);
            assert(d.capture && d.open && !d.tap_r3);
            d = chord.Update(false, false, true);
            assert(!d.capture && !d.open && !d.tap_r3);
        }
    }
    WowChatChord modifier;
    for (int frame = 0; frame < 100; ++frame)
        assert(modifier.Update(true, false, true).capture);
    assert(!modifier.Update(true, true, true).open);
    assert(modifier.Update(false, false, true).open);
    assert(modifier.Update(true, false, true).capture);
    const auto tap = modifier.Update(false, false, true);
    assert(tap.tap_r3 && !tap.open);

    WowChatChord mouse;
    for (int frame = 0; frame < 100; ++frame)
        assert(!mouse.Update(false, true, true).capture); // no delay for ZR alone
    assert(mouse.Update(true, true, true).capture);
    const auto reversed = mouse.Update(false, false, true);
    assert(reversed.open && !reversed.tap_r3); // ZR-first opens the same keyboard
    mouse.Update(true, true, true);
    assert(mouse.Update(false, false, true).open);
    mouse.Update(false, true, true);
    mouse.Update(true, false, true); // R3 pressed as a previous ZR press ends
    assert(mouse.Update(false, false, true).tap_r3);

    // Modal, focus, disconnect or another button cancels an opening. Holding
    // through re-enabling cannot open the keyboard or replay a stick click.
    for (unsigned held = 1; held < 4; ++held)
    {
        WowChatChord disabled;
        disabled.Update(true, true, true);
        auto d = disabled.Update(held & 1, held & 2, false);
        assert(!d.capture && !d.open);
        d = disabled.Update(true, true, true);
        assert(!d.capture && !d.open && !d.tap_r3);
        d = disabled.Update(false, false, true);
        assert(!d.open && !d.tap_r3);
        assert(disabled.Update(true, false, true).capture);
        disabled.Update(true, true, true);
        assert(disabled.Update(false, false, true).open);
    }
}
