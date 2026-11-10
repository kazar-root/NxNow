#include "StreamView.hpp"
#include "localization.hpp"
#include "ui_theme.hpp"

#include <algorithm>

nxnow::input::LauncherMouseDecision StreamView::UpdateLauncherMouse(
    bool enabled, std::chrono::steady_clock::time_point now)
{
    virtual_keys_.reset();
    if (!launcher_mouse_.available())
        return {};
    nxnow::input::LauncherMouseSample sample;
    int source = -1;
    for (size_t i = 0; i < controller_connected_.size(); ++i)
    {
        if (controller_connected_[i])
        {
            source = static_cast<int>(i);
            break;
        }
    }
    if (source != launcher_mouse_source_)
    {
        launcher_mouse_left_desired_ = false;
        launcher_mouse_right_desired_ = false;
        SyncPhysicalMouseButtons();
        launcher_pointer_anchored_ = false;
        SyncRemoteMouseButton(touch_was_down_ || (physical_mouse_buttons_ & 1));
        launcher_mouse_.Update({0, 0, false, false, false, false, 0});
        launcher_mouse_source_ = source;
        launcher_mouse_updated_at_ = now;
    }
    if (source >= 0)
    {
        const auto& state = controller_states_[static_cast<size_t>(source)];
        sample.x = state.axes[brls::LEFT_X];
        sample.y = state.axes[brls::LEFT_Y];
        sample.zr = state.buttons[brls::BUTTON_RT];
        sample.zl = state.buttons[brls::BUTTON_LT];
        sample.l3 = state.buttons[brls::BUTTON_LSB];
        sample.r3 = state.buttons[brls::BUTTON_RSB];
    }
    sample.speed_percent = mouse_speed_percent_;
    sample.enabled = enabled && source >= 0 && session_ && session_->ready_for_input();
    if (launcher_mouse_updated_at_.time_since_epoch().count() != 0)
        sample.elapsed_seconds = std::chrono::duration<double>(now - launcher_mouse_updated_at_).count();
    launcher_mouse_updated_at_ = now;
    const auto decision = launcher_mouse_.Update(sample);
    launcher_mouse_left_desired_ = decision.left_button;
    launcher_mouse_right_desired_ = decision.right_button;
    if (!sample.enabled || decision.toggled)
        launcher_pointer_anchored_ = false;
    if (decision.toggled)
    {
        ReleaseRemoteMouseButton();
        keyboard_release_guard_ = true;
        launcher_hint_.ModeChanged(now);
        if (session_)
            session_->record_ui_event(launcher_mouse_.mouse_mode() ? "launcher mouse enabled" : "gamepad mode enabled");
    }
    if (sample.enabled && launcher_mouse_.mouse_mode() && !decision.toggled && !keyboard_release_guard_)
    {
        const auto& state = controller_states_[static_cast<size_t>(source)];
        virtual_keys_.set(82, state.buttons[brls::BUTTON_UP]);
        virtual_keys_.set(81, state.buttons[brls::BUTTON_DOWN]);
        virtual_keys_.set(80, state.buttons[brls::BUTTON_LEFT]);
        virtual_keys_.set(79, state.buttons[brls::BUTTON_RIGHT]);
        virtual_keys_.set(41, state.buttons[brls::BUTTON_BACK]);
        if (!launcher_pointer_anchored_)
        {
            const float target_x = touch_pointer_initialized_ ? touch_pointer_x_ : session_->negotiated_stream_width() * 0.5f;
            const float target_y = touch_pointer_initialized_ ? touch_pointer_y_ : session_->negotiated_stream_height() * 0.5f;
            ReanchorRemotePointer(target_x, target_y);
            launcher_pointer_anchored_ = true;
        }
        if (decision.dx || decision.dy)
        {
            session_->send_mouse_move(decision.dx, decision.dy);
            touch_pointer_x_ = std::clamp(touch_pointer_x_ + decision.dx, 0.0f,
                static_cast<float>(std::max(1, session_->negotiated_stream_width()) - 1));
            touch_pointer_y_ = std::clamp(touch_pointer_y_ + decision.dy, 0.0f,
                static_cast<float>(std::max(1, session_->negotiated_stream_height()) - 1));
        }
    }
    return decision;
}

void StreamView::SyncRemoteMouseButton(bool desired)
{
    mouse_button_delivery_.Update(desired, [this](bool down) {
        return session_ && session_->send_mouse_left_button(down);
    });
}

void StreamView::ReleaseRemoteMouseButton()
{
    launcher_mouse_left_desired_ = false;
    launcher_mouse_right_desired_ = false;
    virtual_keys_.reset();
    touch_was_down_ = false;
    launcher_pointer_anchored_ = false;
    SyncRemoteMouseButton(false);
    SyncPhysicalMouseButtons();
}

void StreamView::DrawLauncherInputHint(
    NVGcontext* vg, float x, float y, float width, float height,
    std::chrono::steady_clock::time_point now)
{
    if (!launcher_mouse_.available() || keyboard_visible_ || stream_overlay_visible_ ||
        !launcher_hint_.Visible(now))
        return;
    const char* key = launcher_mouse_.mouse_mode()
        ? "Mouse: left stick | ZR = left click | ZL = right click | L3 + R3 = gamepad"
        : is_wow_session_
            ? "WoW: A/B and X/Y swapped | ZR + R3 = chat | L3 + R3 = mouse"
            : "Gamepad mode | L3 + R3 = mouse | R3 + ZR = keyboard";
    const std::string text = nxnow::Tr(key);
    const std::string extra = nxnow::Tr(now < intro_notice_until_
        ? "Escape sent. Confirm in the game if requested."
        : "D-pad = keyboard arrows | Minus = Escape | R3 + ZR = keyboard | USB: automatic");
    const float box_width = std::min(width - 32.0f,
        std::max(nxnow::ui::TextWidth(text, 16, nxnow::ui::FontRole::Medium),
            nxnow::ui::TextWidth(extra, 14, nxnow::ui::FontRole::Medium)) + 36.0f);
    const float left = x + (width - box_width) * 0.5f;
    const float top = y + height - 80.0f;
    nvgSave(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, left, top, box_width, 60, 9);
    nvgFillColor(vg, nvgRGBA(8, 12, 14, 224));
    nvgFill(vg);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Medium));
    nvgFontSize(vg, 16);
    nvgFillColor(vg, nxnow::ui::Text());
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + width * 0.5f, top + 20, text.c_str(), nullptr);
    nvgFontSize(vg, 14);
    nvgFillColor(vg, nxnow::ui::Accent());
    nvgText(vg, x + width * 0.5f, top + 43, extra.c_str(), nullptr);
    nvgRestore(vg);
}

