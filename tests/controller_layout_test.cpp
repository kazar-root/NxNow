#include "controller_layout.hpp"

#include <cassert>

int main()
{
    using namespace nxnow;

    assert(MapFaceButtons("Xbox", true, false, false, false) == kXboxButtonB);
    assert(MapFaceButtons("Xbox", false, true, false, false) == kXboxButtonA);
    assert(MapFaceButtons("Xbox", false, false, true, false) == kXboxButtonY);
    assert(MapFaceButtons("Xbox", false, false, false, true) == kXboxButtonX);

    assert(MapFaceButtons("Switch", true, false, false, false) == kXboxButtonA);
    assert(MapFaceButtons("Switch", false, true, false, false) == kXboxButtonB);
    assert(MapFaceButtons("Switch", false, false, true, false) == kXboxButtonX);
    assert(MapFaceButtons("Switch", false, false, false, true) == kXboxButtonY);

    assert(MapFaceButtons("Switch", true, true, true, true) ==
           (kXboxButtonA | kXboxButtonB | kXboxButtonX | kXboxButtonY));
    assert(MapFaceButtons("invalid", true, false, false, false) == kXboxButtonA);

    // Every independent/combined shoulder state must keep bumpers and full
    // range digital triggers in distinct Xbox report fields.
    for (unsigned mask = 0; mask < 16; ++mask)
    {
        const auto mapped = MapShoulderInputs(mask & 1, mask & 2, mask & 4, mask & 8);
        assert(mapped.buttons == ((mask & 1 ? 0x0100 : 0) | (mask & 2 ? 0x0200 : 0)));
        assert(mapped.left_trigger == (mask & 4 ? 255 : 0));
        assert(mapped.right_trigger == (mask & 8 ? 255 : 0));
    }
    return 0;
}
