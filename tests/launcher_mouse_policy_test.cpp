#include "launcher_mouse_policy.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <string_view>

using namespace nxnow::input;

int main()
{
    LauncherMouseController ordinary;
    assert(!ordinary.available() && !ordinary.mouse_mode());
    assert(!ordinary.Update({1, 1, true, true, true, true, 1}).capture);

    LauncherMouseController initial(true);
    assert(initial.available() && !initial.mouse_mode());
    assert(!initial.Update({1, 1, true}).capture);
    assert(std::string_view(InputModeLabel(initial.mouse_mode())) == "Gamepad mode");
    assert(std::string_view(InputModeLabel(true)) == "Keyboard / mouse mode");

    LauncherMouseController mouse(true, true);
    assert(mouse.available() && mouse.mouse_mode());
    // A trigger carried through launch, reconnect or a modal never clicks.
    assert(!mouse.Update({0, 0, true}).left_button);
    mouse.Update({});
    assert(mouse.Update({0, 0, true}).left_button);
    assert(!mouse.Update({}).left_button);
    mouse.Update({0, 0, true, false, false, false});
    assert(!mouse.Update({0, 0, true}).left_button);
    mouse.Update({});
    assert(mouse.Update({0, 0, true}).left_button);

    LauncherMouseController universal(true, false);
    assert(universal.available() && !universal.mouse_mode());
    universal.Update({0, 0, false, true, true});
    assert(universal.Update({}).toggled && universal.mouse_mode());
    universal.Update({});
    LauncherMouseSample left;
    left.zr = true;
    const auto left_click = universal.Update(left);
    assert(left_click.left_button && !left_click.right_button);
    left.zl = true;
    const auto both = universal.Update(left);
    assert(both.left_button && both.right_button);
    universal.Update({0, 0, false, false, false, false});
    assert(!universal.Update(left).left_button && !universal.Update(left).right_button);
    universal.Update({});
    LauncherMouseSample fast;
    fast.x = 1; fast.elapsed_seconds = .02; fast.speed_percent = 200;
    assert(universal.Update(fast).dx == 40);
    fast.speed_percent = 50;
    assert(universal.Update(fast).dx == 10);

    // Toggle only once, after both stick buttons are released, and capture
    // each chord frame so no thumb-stick presses reach the game.
    assert(mouse.Update({0, 0, false, true, true}).capture);
    for (int frame = 0; frame < 100; ++frame)
        assert(!mouse.Update({0, 0, false, true, true}).toggled);
    assert(!mouse.Update({0, 0, false, false, true}).toggled);
    const auto gamepad = mouse.Update({});
    assert(gamepad.capture && gamepad.toggled && !mouse.mouse_mode());
    assert(!mouse.Update({1, 1, true, false, false, true, .02}).capture);
    mouse.Update({0, 0, false, true, true});
    const auto pointer = mouse.Update({0, 0, true});
    assert(pointer.toggled && mouse.mouse_mode() && !pointer.left_button);
    assert(!mouse.Update({0, 0, true}).left_button);
    mouse.Update({});
    assert(mouse.Update({0, 0, true}).left_button);

    // A modal cancels an armed toggle; buttons held across focus recovery
    // cannot accidentally select another mode.
    mouse.Update({0, 0, false, true, true});
    mouse.Update({0, 0, false, true, true, false});
    assert(!mouse.Update({0, 0, false, true, true}).toggled);
    assert(!mouse.Update({}).toggled && mouse.mouse_mode());
    mouse.UseGamepad();
    assert(!mouse.mouse_mode() && !mouse.Update({}).capture);

    LauncherMouseController drift(true, true);
    for (int frame = 0; frame < 600; ++frame)
    {
        const auto idle = drift.Update({.1f, -.1f, false, false, false, true, .02});
        assert(idle.dx == 0 && idle.dy == 0);
    }
    auto movement = drift.Update({1, 0, false, false, false, true, .02});
    assert(movement.dx == 20 && movement.dy == 0);
    movement = drift.Update({0, -1, false, false, false, true, .02});
    assert(movement.dx == 0 && movement.dy == -20);
    movement = drift.Update({1, 1, false, false, false, true, .02});
    assert(movement.dx > 0 && movement.dy > 0 && movement.dx <= 15 && movement.dy <= 15);
    movement = drift.Update({1, 0, false, false, false, true, 500});
    assert(movement.dx <= 50); // UI stalls cannot produce a huge cursor jump.
    movement = drift.Update({std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), false, false, false, true, .02});
    assert(movement.dx == 0 && movement.dy == 0);
    movement = drift.Update({1, 0, false, false, false, true,
        std::numeric_limits<double>::infinity()});
    assert(movement.dx == 0 && movement.dy == 0);

    auto travel = [](int frames, float deflection) {
        LauncherMouseController controller(true, true);
        int pixels = 0;
        for (int frame = 0; frame < frames; ++frame)
            pixels += controller.Update({deflection, 0, false, false, false, true,
                1.0 / frames}).dx;
        return pixels;
    };
    assert(std::abs(travel(30, 1) - travel(60, 1)) <= 1);
    assert(travel(60, 1) >= 999);
    const int slow = travel(60, .2f);
    assert(slow >= 2 && slow <= 3); // Subpixel motion accumulates for precision.

    MouseEscapeKey escape;
    assert(!escape.Update(true, false, true));
    assert(escape.Update(false, false, true));
    assert(!escape.Update(false, false, true));
    assert(!escape.Update(true, true, true)); // Plus+Minus never sends Escape.
    assert(!escape.Update(true, false, true));
    assert(!escape.Update(false, false, true));
    assert(!escape.Update(true, false, false)); // No held key from a modal.
    assert(!escape.Update(false, false, true));

    // Keep retrying button transitions until the transport accepts them;
    // steady holds do not flood the reliable channel.
    MouseButtonDelivery delivery;
    std::vector<bool> packets;
    bool accepted = false;
    auto send = [&](bool down) { packets.push_back(down); return accepted; };
    delivery.Update(false, send);
    assert(packets.empty());
    delivery.Update(true, send);
    assert(!delivery.held());
    accepted = true;
    delivery.Update(true, send);
    assert(delivery.held());
    delivery.Update(true, send);
    assert(packets.size() == 2);
    accepted = false;
    delivery.Update(false, send);
    assert(delivery.held());
    accepted = true;
    delivery.Update(false, send);
    assert(!delivery.held() && packets == (std::vector<bool>{true, true, false, false}));
}
