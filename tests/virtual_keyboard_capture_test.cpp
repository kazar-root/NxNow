#include "virtual_keyboard_capture.hpp"
#include "controller_delivery_policy.hpp"
#include "launcher_mouse_policy.hpp"
#include "deferred_keyboard_input.hpp"
#include <cassert>
#include <string_view>
#include <vector>
using namespace nxnow::input;
int main()
{
    using namespace std::chrono;
    const auto start = VirtualKeyboardCapture::Clock::time_point{} + seconds(10);
    VirtualKeyboardCapture gate;
    LauncherMouseController controller(true);
    assert(!gate.active() && !controller.mouse_mode());
    gate.Begin(start);
    assert(gate.Update(true, true, false, start));
    assert(std::string_view(InputModeLabel(controller.mouse_mode())) == "Gamepad mode");

    GamepadPresence neutral;
    std::array<bool, kRemoteControllerCount> connected {}; connected[0] = true;
    int attempts = 0;
    neutral.Update(true, connected, [&](auto, auto) { ++attempts; return false; });
    assert(!neutral.Complete(connected));
    DeferredKeyboardInput text;
    text.Append({0x0d, 0x1c, 0}); // Open WoW chat before typed characters.
    assert(text.AppendEdit("", "azerty", KeyboardLayout::French));
    std::vector<KeyboardEvent> delivered;
    text.Drain(neutral.Complete(connected), [&](auto event) { delivered.push_back(event); return true; });
    assert(delivered.empty());
    neutral.Update(true, connected, [&](auto id, auto mask) { ++attempts; assert(id == 0 && mask == 1); return true; });
    assert(neutral.Complete(connected));
    for (int frame = 0; frame < 300; ++frame)
    {
        assert(gate.Update(true, false, text.size() != 0, start + milliseconds(frame * 16)));
        neutral.Update(true, connected, [&](auto, auto) { ++attempts; return true; });
        text.Drain(neutral.Complete(connected), [&](auto event) { delivered.push_back(event); return true; });
    }
    assert(attempts == 2); // One successful neutral state; no idle report stealing text focus.
    assert(delivered.front().stroke.keycode == 0x0d && delivered.front().pressed);
    assert(delivered[1].stroke.keycode == 0x0d && !delivered[1].pressed);
    assert(!controller.mouse_mode());
    const auto close = start + seconds(6); gate.Quiet(close);
    assert(gate.Update(false, true, false, close + seconds(1)));
    assert(gate.Update(false, false, true, close + seconds(2)));
    assert(gate.Update(false, false, false, close + seconds(2) + milliseconds(149)));
    assert(!gate.Update(false, false, false, close + seconds(2) + milliseconds(150)));
    assert(!gate.active() && !controller.mouse_mode());
}
