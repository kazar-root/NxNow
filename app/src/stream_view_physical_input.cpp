#include "StreamView.hpp"

#include <algorithm>

namespace
{
constexpr auto kInputStaleTime = std::chrono::milliseconds(500);

#ifdef __SWITCH__
nxnow::input::PhysicalKeys KeysFromHid(const HidKeyboardState& state)
{
    nxnow::input::PhysicalKeys keys;
    for (unsigned key = 0; key < 256; ++key)
        keys[key] = (state.keys[key / 64] & (std::uint64_t{1} << (key % 64))) != 0;
    nxnow::input::AddPhysicalModifiers(keys, state.modifiers);
    return keys;
}
#endif
}

void StreamView::DeliverPhysicalKeyboard(bool enabled, bool escape_pulse)
{
    auto desired = physical_keyboard_capture_.Update(physical_keys_, enabled);
    desired |= virtual_keys_;
    if (escape_pulse)
        desired.set(41);
    physical_keyboard_delivery_.Update(desired, [this](const auto& stroke, bool down) {
        return session_ && session_->send_keyboard_key(stroke.keycode, stroke.scancode, stroke.modifiers, down);
    });
}

void StreamView::SyncPhysicalMouseButtons()
{
    // HID left/right/middle/forward/back -> NVST left/middle/right/back/forward.
    constexpr unsigned masks[] = {4, 2, 16, 8};
    for (std::size_t i = 0; i < physical_mouse_delivery_.size(); ++i)
        physical_mouse_delivery_[i].Update((physical_mouse_buttons_ & masks[i]) != 0 ||
            (i == 1 && launcher_mouse_right_desired_),
            [this, i](bool down) { return session_ && session_->send_mouse_button(static_cast<uint8_t>(i + 2), down); });
    SyncRemoteMouseButton(touch_was_down_ || launcher_mouse_left_desired_ || (physical_mouse_buttons_ & 1));
}

void StreamView::ReleasePhysicalInputs()
{
    intro_escape_until_ = {};
    mouse_escape_until_ = {};
    intro_start_until_ = {};
    wow_r3_until_ = {};
    virtual_keys_.reset();
    DeliverPhysicalKeyboard(false, false);
    physical_mouse_capture_.Update(physical_mouse_held_, false);
    physical_mouse_buttons_ = 0;
    physical_wheel_pending_ = 0;
    SyncPhysicalMouseButtons();
    physical_keyboard_sequence_ = {};
    physical_mouse_sequence_ = {};
    physical_keyboard_seen_at_ = {};
    physical_mouse_seen_at_ = {};
}

void StreamView::UpdatePhysicalInputs(bool enabled, bool escape_pulse,
                                      std::chrono::steady_clock::time_point now)
{
    enabled = enabled && session_ && session_->ready_for_input();
#ifdef __SWITCH__
    std::array<HidKeyboardState, 17> keyboard;
    const auto keyboard_count = hidGetKeyboardStates(keyboard.data(), keyboard.size());
    const bool stale_keyboard = now - physical_keyboard_seen_at_ > kInputStaleTime;
    if (keyboard_count == 0)
        physical_keys_.reset();
    else if (!physical_keyboard_sequence_.initialized() || stale_keyboard ||
             physical_keyboard_sequence_.Restarted(keyboard[0].sampling_number))
    {
        // Newest first in libnx. Baseline before replaying future samples so
        // opening the stream cannot replay keys from the menu or an old device.
        physical_keyboard_sequence_.Baseline(keyboard[0].sampling_number);
        physical_keyboard_seen_at_ = now;
        physical_keys_ = KeysFromHid(keyboard[0]);
        DeliverPhysicalKeyboard(false, escape_pulse);
    }
    else
    {
        for (std::size_t i = keyboard_count; i-- > 0;)
        {
            if (!physical_keyboard_sequence_.Accept(keyboard[i].sampling_number))
                continue;
            physical_keyboard_seen_at_ = now;
            physical_keys_ = KeysFromHid(keyboard[i]);
            DeliverPhysicalKeyboard(enabled, escape_pulse);
        }
    }
    DeliverPhysicalKeyboard(enabled && now - physical_keyboard_seen_at_ <= kInputStaleTime, escape_pulse);

    std::array<HidMouseState, 17> mouse;
    const auto mouse_count = hidGetMouseStates(mouse.data(), mouse.size());
    const bool stale_mouse = now - physical_mouse_seen_at_ > kInputStaleTime;
    if (mouse_count == 0)
        physical_mouse_held_ = 0;
    else if (!physical_mouse_sequence_.initialized() || stale_mouse ||
             physical_mouse_sequence_.Restarted(mouse[0].sampling_number))
    {
        physical_mouse_sequence_.Baseline(mouse[0].sampling_number);
        physical_mouse_seen_at_ = now;
        physical_mouse_held_ = (mouse[0].attributes & HidMouseAttribute_IsConnected) ? mouse[0].buttons : 0;
        physical_mouse_capture_.Update(physical_mouse_held_, false);
        physical_mouse_buttons_ = 0;
    }
    else
    {
        for (std::size_t i = mouse_count; i-- > 0;)
        {
            const auto& sample = mouse[i];
            if (!physical_mouse_sequence_.Accept(sample.sampling_number))
                continue;
            physical_mouse_seen_at_ = now;
            const bool connected = sample.attributes & HidMouseAttribute_IsConnected;
            physical_mouse_held_ = connected ? sample.buttons : 0;
            physical_mouse_buttons_ = physical_mouse_capture_.Update(physical_mouse_held_, enabled);
            if (enabled && connected)
            {
                const auto dx = static_cast<int16_t>(std::clamp(sample.delta_x, -4096, 4096));
                const auto dy = static_cast<int16_t>(std::clamp(sample.delta_y, -4096, 4096));
                if (dx || dy)
                {
                    session_->send_mouse_move(dx, dy);
                    touch_pointer_x_ = std::clamp(touch_pointer_x_ + dx, 0.0f,
                        static_cast<float>(std::max(1, session_->negotiated_stream_width()) - 1));
                    touch_pointer_y_ = std::clamp(touch_pointer_y_ + dy, 0.0f,
                        static_cast<float>(std::max(1, session_->negotiated_stream_height()) - 1));
                }
                // Same vertical-wheel field as the Switch Borealis backend.
                physical_wheel_pending_ = static_cast<int>(std::clamp<std::int64_t>(
                    physical_wheel_pending_ + nxnow::input::PhysicalWheelDelta(sample.wheel_delta_x), -960, 960));
            }
            SyncPhysicalMouseButtons();
        }
    }
    const bool fresh_mouse = now - physical_mouse_seen_at_ <= kInputStaleTime;
    physical_mouse_buttons_ = physical_mouse_capture_.Update(physical_mouse_held_, enabled && fresh_mouse);
    if (!enabled || !fresh_mouse)
        physical_wheel_pending_ = 0;
    if (physical_wheel_pending_ && session_->send_mouse_wheel(static_cast<int16_t>(physical_wheel_pending_)))
        physical_wheel_pending_ = 0;
#else
    (void)now;
    DeliverPhysicalKeyboard(enabled, escape_pulse);
#endif
    SyncPhysicalMouseButtons();
}
