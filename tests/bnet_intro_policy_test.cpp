#include "bnet_intro_policy.hpp"
#include "stream_overlay_policy.hpp"
#include <algorithm>
#include <array>
#include <cassert>

int main()
{
    using namespace nxnow::input;
    BnetIntroShortcut hold;
    assert(hold.Update(true, false, true, 0).capture);
    assert(!hold.Update(true, false, true, 999).fire);
    auto action = hold.Update(true, false, true, 1000);
    assert(action.fire && action.capture && !action.tap_start);
    assert(!hold.Update(true, false, true, 100000).fire);
    assert(!hold.Update(false, false, true, 100001).tap_start);
    hold.Update(true, false, true, 100002);
    assert(hold.Update(true, false, true, 101002).fire);
    hold.Update(false, false, true, 101003);
    hold.Update(true, false, true, 101004);
    action = hold.Update(false, false, true, 101100);
    assert(action.tap_start && !action.fire && action.capture);
    assert(!hold.Update(false, false, true, 102000).tap_start);

    // An irregular frame interval can cross the threshold during release.
    hold.Update(true, false, true, 103000);
    assert(hold.Update(false, false, true, 104500).fire);

    for (bool enabled : {false, true})
    {
        BnetIntroShortcut blocked;
        blocked.Update(true, false, true, 0);
        blocked.Update(true, true, enabled, 500);
        assert(!blocked.Update(true, false, true, 2000).fire);
        assert(!blocked.Update(false, false, true, 2001).tap_start);
        blocked.Update(true, false, true, 2002);
        blocked.Update(true, false, false, 2500); // keyboard, focus loss or disconnect
        assert(!blocked.Update(true, false, true, 5000).fire);
        assert(!blocked.Update(false, false, true, 5001).fire);
        blocked.Update(true, false, true, 5002);
        assert(blocked.Update(true, false, true, 6002).fire);
    }

    // Exercise the actual two policies together: stats and the four-button
    // exit consume their entire chord, including either release order.
    std::array press {1, 2, 4, 8};
    do {
        BnetIntroShortcut intro;
        StreamShortcutChord shortcuts;
        int held = 0, exits = 0, skips = 0;
        std::int64_t now = 0;
        auto poll = [&] {
            const auto s = shortcuts.Update(held & 2, held & 1, held & 4, held & 8, true, now);
            const auto i = intro.Update(held & 1, held & 14, true, now);
            exits += s.exit_stream;
            skips += i.fire;
            assert(!i.tap_start);
            now += 200;
        };
        for (int button : press) { held |= button; poll(); }
        for (int frame = 0; frame < 20; ++frame) poll();
        for (int button : press) { held &= ~button; poll(); }
        assert(exits == 1 && skips == 0);
    } while (std::next_permutation(press.begin(), press.end()));
    for (bool plus_first : {false, true})
    {
        BnetIntroShortcut intro;
        StreamShortcutChord shortcuts;
        intro.Update(plus_first, !plus_first, true, 0);
        shortcuts.Update(!plus_first, plus_first, false, false, true, 0);
        for (int now = 100; now <= 3000; now += 100)
        {
            assert(!intro.Update(true, true, true, now).fire);
            shortcuts.Update(true, true, false, false, true, now);
        }
        assert(shortcuts.Update(false, false, false, false, true, 3001).toggle_stats);
        const auto result = intro.Update(false, false, true, 3001);
        assert(!result.fire && !result.tap_start);
    }
    BnetIntroShortcut ordinary;
    assert(!ordinary.Update(true, false, false, 0).capture);
    assert(!ordinary.Update(true, false, false, 2000).fire);
}
