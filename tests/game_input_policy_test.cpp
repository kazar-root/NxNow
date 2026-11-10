#include "controller_layout.hpp"
#include "game_input_policy.hpp"

#include <cassert>

int main()
{
    using namespace nxnow;
    using namespace nxnow::input;
    for (const auto* store : {"BNET", "Battle.net", "battle net", "BLIZZARD", "Blizzard Battle.net"})
        assert(IsBattleNetStore(store));
    for (const auto* store : {"", "Steam", "Xbox", "Ubisoft Connect", "not-bnet"})
        assert(!IsBattleNetStore(store));
    for (const auto* title : {"World of Warcraft", "World of Warcraft Classic",
             "World of Warcraft®: The War Within", "WoW", "WOW Classic", "wow-retail"})
    {
        assert(IsWorldOfWarcraft(title));
        // Both saved layouts converge on exactly one swap for the requested
        // WoW behavior, including simultaneous face-button presses.
        for (const auto* selected : {"Switch", "Xbox"})
        {
            const auto layout = ControllerLayoutForGame(title, selected);
            for (int held = 0; held < 16; ++held)
                assert(MapFaceButtons(layout, held & 1, held & 2, held & 4, held & 8) ==
                    ((held & 1 ? kXboxButtonB : 0) | (held & 2 ? kXboxButtonA : 0) |
                     (held & 4 ? kXboxButtonY : 0) | (held & 8 ? kXboxButtonX : 0)));
        }
    }
    for (const auto* title : {"Warcraft III", "Diablo IV", "Overwatch 2", "Wowhead", ""})
    {
        assert(!IsWorldOfWarcraft(title));
        for (const auto* selected : {"Switch", "Xbox"})
            assert(ControllerLayoutForGame(title, selected) == selected);
    }
}
