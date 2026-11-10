#include "controller_delivery_policy.hpp"
#include <cassert>
using namespace nxnow::input;
int main()
{
    GamepadPresence presence; std::array<bool, kRemoteControllerCount> connected {};
    connected[0] = true; int calls = 0; bool accept = false;
    auto send = [&](uint8_t id, uint16_t bitmap) { ++calls; assert(id == 0 && bitmap == 1); return accept; };
    presence.Update(false, connected, send); assert(calls == 0);
    presence.Update(true, connected, send); assert(calls == 1);
    accept = true; presence.Update(true, connected, send); assert(calls == 2);
    for (int i = 0; i < 100; ++i) presence.Update(true, connected, send);
    assert(calls == 2); // Announce before any button press; do not flood.
    presence.Update(false, connected, send); presence.Update(true, connected, send); assert(calls == 3);
    presence.Update(false, connected, send);
    presence.Delivered(0); // An accepted real report also establishes presence.
    presence.Update(true, connected, send); assert(calls == 3);
    connected[0] = false; presence.Update(true, connected, send);
    connected[0] = true; presence.Update(true, connected, send); assert(calls == 4);
}
