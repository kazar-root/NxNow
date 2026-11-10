#include "stream_overlay_policy.hpp"

#include <algorithm>
#include <array>
#include <cassert>

int main()
{
    using nxnow::input::StreamShortcutChord;
    // Every press/release ordering of the four-button exit, including a
    // deliberately slow press, fires once and never toggles the statistics.
    for (bool allow_stats : {false, true})
    {
        std::array press {1, 2, 4, 8};
        do
        {
            std::array release {1, 2, 4, 8};
            do
            {
                StreamShortcutChord chord;
                int held = 0;
                int exits = 0;
                int toggles = 0;
                std::int64_t now = 0;
                auto update = [&] {
                    const auto decision = chord.Update(
                        held & 1, held & 2, held & 4, held & 8, allow_stats, now);
                    now += 250;
                    exits += decision.exit_stream;
                    toggles += decision.toggle_stats;
                    return decision;
                };
                for (int button : press)
                {
                    held |= button;
                    update();
                    if (held != 15)
                        assert(exits == 0);
                }
                assert(exits == 1);
                for (int frame = 0; frame < 60; ++frame)
                    assert(update().capture);
                for (int button : release)
                {
                    held &= ~button;
                    assert(update().capture);
                }
                assert(exits == 1 && toggles == 0);
                assert(!update().capture);
                held = 15;
                assert(update().exit_stream);
                assert(exits == 2 && toggles == 0);
            } while (std::next_permutation(release.begin(), release.end()));
        } while (std::next_permutation(press.begin(), press.end()));
    }
    // Two-button stats toggle: either order, either release order, no repeat
    // while held, and no input leakage between the two releases.
    for (int first : {1, 2, 3})
    {
        for (int release_first : {1, 2})
        {
            StreamShortcutChord chord;
            std::int64_t now = 1000;
            for (int repeat = 0; repeat < 3; ++repeat)
            {
                auto poll = [&](int held) {
                    return chord.Update(held & 1, held & 2, false, false, true, now++);
                };
                assert(!poll(first).toggle_stats);
                assert(!poll(3).toggle_stats);
                for (int frame = 0; frame < 120; ++frame)
                {
                    const auto d = poll(3);
                    assert(d.capture && !d.toggle_stats && !d.exit_stream);
                }
                assert(!poll(3 & ~release_first).toggle_stats);
                auto d = poll(0);
                assert(d.toggle_stats && d.capture && !d.exit_stream);
                d = poll(0);
                assert(!d.toggle_stats && !d.capture);
            }
        }
    }
    StreamShortcutChord chord;
    assert(chord.Update(true, false, false, false, true, 0).capture);
    assert(!chord.Update(true, false, false, false, true, 120).capture);
    assert(!chord.Update(true, true, false, false, true, 130).toggle_stats);
    assert(!chord.Update(false, false, false, false, true, 140).toggle_stats);
    // The former three-button exit must no longer exit.
    assert(!chord.Update(true, false, true, true, true, 150).exit_stream);
    assert(!chord.Update(false, false, false, false, true, 160).toggle_stats);
    // A modal appearing mid-chord cancels the toggle, including on release.
    assert(chord.Update(true, true, false, false, true, 200).capture);
    assert(!chord.Update(true, true, false, false, false, 210).toggle_stats);
    assert(!chord.Update(false, false, false, false, true, 220).toggle_stats);
    assert(!chord.Update(false, false, false, false, true, 230).capture);
}
