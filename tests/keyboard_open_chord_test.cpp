#include "controller_delivery_policy.hpp"
#include "keyboard_input_policy.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

int main()
{
    using namespace nxnow::input;

    // All six press orders and all six release orders work. The keyboard
    // waits for MINUS, ZR and Y to be released, even across different frames.
    std::array press_order {1, 2, 4};
    do {
        std::array release_order {1, 2, 4};
        do
        {
            KeyboardOpenChord chord;
            int held = 0;
            for (int button : press_order)
            {
                held |= button;
                const auto decision = chord.Update(held & 1, held & 2, held & 4, true);
                assert(!decision.open && decision.capture == (held == 7));
            }
            for (int frame = 0; frame < 120; ++frame)
            {
                const auto decision = chord.Update(true, true, true, true);
                assert(!decision.open && decision.capture);
            }
            for (int button : release_order)
            {
                held &= ~button;
                const auto decision = chord.Update(held & 1, held & 2, held & 4, true);
                assert(decision.open == (held == 0) && decision.capture);
            }
            auto decision = chord.Update(false, false, false, true);
            assert(!decision.open && !decision.capture);
            // The next complete press/release opens exactly once again.
            decision = chord.Update(true, true, true, true);
            assert(!decision.open && decision.capture);
            decision = chord.Update(false, false, false, true);
            assert(decision.open && decision.capture);
        } while (std::next_permutation(release_order.begin(), release_order.end()));
    } while (std::next_permutation(press_order.begin(), press_order.end()));

    // Same-frame chord works. Every incomplete combination stays available to
    // gameplay, including ZR + Y without MINUS, and MINUS + Y without ZR.
    KeyboardOpenChord chord;
    assert(chord.Update(true, true, true, true).capture);
    assert(chord.Update(false, false, false, true).open);
    for (int held = 1; held < 7; ++held)
    {
        for (int frame = 0; frame < 120; ++frame)
        {
            const auto decision = chord.Update(held & 1, held & 2, held & 4, true);
            assert(!decision.open && !decision.capture);
        }
        assert(!chord.Update(false, false, false, true).open);
    }

    // Focus loss, menu, automation, disconnect, and the post-keyboard release
    // guard all disable the chord. Holding any button through re-enabling
    // cannot turn an old gameplay press into an opening request.
    for (int held = 1; held <= 7; ++held)
    {
        KeyboardOpenChord blocked;
        const auto disabled = blocked.Update(held & 1, held & 2, held & 4, false);
        assert(!disabled.open && !disabled.capture);
        auto decision = blocked.Update(true, true, true, true);
        assert(!decision.open && !decision.capture);
        decision = blocked.Update(false, true, true, true);
        assert(!decision.open && !decision.capture);
        assert(!blocked.Update(false, false, false, true).open);
        assert(blocked.Update(true, true, true, true).capture);
        assert(blocked.Update(false, false, false, true).open);
    }

    // A context change also cancels a pending opening, even if the buttons
    // are released in the very frame that input becomes disabled.
    for (bool held_on_disable : {false, true})
    {
        KeyboardOpenChord cancelled;
        assert(cancelled.Update(true, true, true, true).capture);
        assert(!cancelled.Update(held_on_disable, held_on_disable, held_on_disable, false).open);
        assert(!cancelled.Update(false, false, false, true).open);
        assert(cancelled.Update(true, true, true, true).capture);
        assert(cancelled.Update(false, false, false, true).open);
    }

    // Entering capture neutralizes previously held remote controls, including
    // a ZR that reached the game before Y was pressed, and clears system pulses.
    ControllerInputCapture capture;
    std::array<ControllerDeliveryState, kRemoteControllerCount> delivery {};
    delivery[0].initialized = true;
    delivery[0].last_buttons = 0x1000;
    delivery[0].last_right_trigger = 255;
    const auto now = std::chrono::steady_clock::now();
    delivery[0].back_pulse.Queue(now);
    KeyboardOpenChord captured;
    assert(capture.Update(captured.Update(true, true, true, true).capture, delivery));
    assert(delivery[0].last_buttons == 0 && delivery[0].last_right_trigger == 0);
    assert(delivery[0].SystemButtons(now) == 0);
    bool sent = false;
    DeliverNeutralControllerReports({true, false, false, false}, delivery, now,
        [&sent](std::uint8_t controller, std::uint16_t) {
            assert(controller == 0);
            sent = true;
            return true;
        });
    assert(sent);
    assert(!capture.Update(captured.Update(false, true, true, true).capture, delivery));
    assert(!capture.Update(captured.Update(false, false, false, true).capture, delivery));
    assert(capture.Update(false, delivery));
}
