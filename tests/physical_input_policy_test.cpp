#include "physical_input_policy.hpp"
#include "launcher_mouse_policy.hpp"
#include <cassert>
#include <vector>

int main()
{
    using namespace nxnow::input;
    for (unsigned key = 4; key <= 29; ++key)
    {
        const auto stroke = PhysicalKeyStroke(key);
        assert(stroke.keycode == 'A' + key - 4 && stroke.scancode != 0);
    }
    assert(PhysicalKeyStroke(41).keycode == 0x1b && PhysicalKeyStroke(41).scancode == 1);
    assert(PhysicalKeyStroke(44).keycode == 0x20);
    assert(PhysicalKeyStroke(40).keycode == 0x0d);
    assert(PhysicalKeyStroke(58).keycode == 0x70 && PhysicalKeyStroke(69).keycode == 0x7b);
    assert(PhysicalKeyStroke(115).keycode == 0x87);
    assert(PhysicalKeyStroke(79).keycode == 0x27 && PhysicalKeyStroke(89).keycode == 0x61);
    assert(PhysicalKeyStroke(0).keycode == 0 && PhysicalKeyStroke(255).keycode == 0);

    PhysicalKeys held;
    held.set(228); // right control stays right control with aggregate Ctrl set
    AddPhysicalModifiers(held, 1 | 2 | 8 | 16);
    assert(held[228] && !held[224] && held[225] && held[230] && held[227]);
    assert(PhysicalModifiers(held) == 15);

    PhysicalKeyboardCapture capture;
    held.reset(); held.set(26); // W held while a modal appears
    assert(capture.Update(held, false).none());
    assert(capture.Update(held, true).none());
    held.reset(); assert(capture.Update(held, true).none());
    held.set(26); assert(capture.Update(held, true)[26]);

    PhysicalKeyboardDelivery delivery;
    std::vector<KeyboardEvent> events;
    bool accepted = true;
    auto send = [&](KeyboardStroke stroke, bool down) {
        events.push_back({stroke, down}); return accepted;
    };
    held.reset(); held.set(4); held.set(224); // Ctrl+A even if A has a lower HID usage
    assert(delivery.Update(held, send));
    assert(events.size() == 2 && events[0].stroke.keycode == 0xa2 && events[0].pressed);
    assert(events[1].stroke.keycode == 'A' && events[1].stroke.modifiers == 2 && events[1].pressed);
    delivery.Update(held, send);
    assert(events.size() == 2); // no per-frame down repeats
    accepted = false;
    assert(!delivery.Update({}, send) && delivery.any());
    assert(events.back().stroke.keycode == 'A' && !events.back().pressed);
    accepted = true;
    assert(delivery.Update({}, send) && !delivery.any());
    assert(events[3].stroke.keycode == 'A' && !events[3].pressed && events[3].stroke.modifiers == 2);
    assert(events[4].stroke.keycode == 0xa2 && !events[4].pressed && events[4].stroke.modifiers == 0);

    held.reset(); held.set(225); held.set(229);
    delivery.Update(held, send);
    held.reset(225); delivery.Update(held, send);
    assert(events.back().stroke.keycode == 0xa0 && events.back().stroke.modifiers == 1);
    held.reset(); delivery.Update(held, send);
    assert(events.back().stroke.keycode == 0xa1 && events.back().stroke.modifiers == 0);

    // Synthetic Escape and a physical Escape share one delivered state.
    held.set(41); delivery.Update(held, send);
    const auto escape_down = events.size();
    delivery.Update(held, send); assert(events.size() == escape_down);
    delivery.Update({}, send);
    assert(!events.back().pressed && events.back().stroke.keycode == 0x1b);

    PhysicalMouseCapture mouse;
    assert(mouse.Update(3, false) == 0);
    assert(mouse.Update(3, true) == 0);
    assert(mouse.Update(0, true) == 0);
    assert(mouse.Update(3, true) == 3);
    assert(mouse.Update(0, true) == 0); // unplug releases all buttons
    assert(PhysicalWheelDelta(1) == 120 && PhysicalWheelDelta(-1) == -120);
    assert(PhysicalWheelDelta(120) == 120 && PhysicalWheelDelta(-240) == -240);
    assert(PhysicalWheelDelta(2147483647) == 960);

    HidSampleSequence sequence;
    assert(!sequence.initialized() && !sequence.Accept(10));
    sequence.Baseline(10);
    for (unsigned sample : {7, 8, 9, 10}) assert(!sequence.Accept(sample));
    assert(sequence.Accept(11) && !sequence.Accept(11) && sequence.Accept(12));
    assert(sequence.Restarted(1));
    sequence.Baseline(1);
    assert(sequence.Accept(2));
}
