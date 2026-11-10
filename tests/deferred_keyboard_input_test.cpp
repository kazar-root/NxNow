#include "deferred_keyboard_input.hpp"
#include <cassert>
#include <vector>
using namespace nxnow::input;
int main()
{
    DeferredKeyboardInput pending;
    assert(pending.AppendText("Ab1"));
    const auto size = pending.size();
    std::vector<KeyboardEvent> sent;
    auto send = [&](auto event) { sent.push_back(event); return true; };
    pending.Drain(false, send);
    assert(sent.empty() && pending.size() == size);
    pending.Drain(true, [](auto) { return false; });
    assert(pending.size() == size); // Congestion retains the key-down and key-up.
    pending.Drain(true, send);
    assert(pending.size() == 0 && sent.size() == size);
    assert(sent.back().stroke.keycode == 0x0d && !sent.back().pressed);
    assert(!pending.AppendText("é") && !pending.AppendText(std::string(501, 'a')));
    while (pending.Append({0x41, 0x1e, 0})) {}
    assert(pending.size() <= DeferredKeyboardInput::kMaxEvents);
    for (int i = 0; i < 100; ++i) assert(!pending.Append({0x41, 0x1e, 0}));
    pending.Clear(); assert(pending.size() == 0);
}
