#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nxnow::input
{
inline const char* InputModeLabel(bool mouse_mode)
{
    return mouse_mode ? "Keyboard / mouse mode" : "Gamepad mode";
}

struct LauncherMouseSample
{
    float x = 0;
    float y = 0;
    bool zr = false;
    bool l3 = false;
    bool r3 = false;
    bool enabled = true;
    double elapsed_seconds = 0;
    bool zl = false;
    int speed_percent = 100;
};

struct LauncherMouseDecision
{
    bool capture = false;
    bool toggled = false;
    bool left_button = false;
    std::int16_t dx = 0;
    std::int16_t dy = 0;
    bool right_button = false;
};

class LauncherMouseController
{
  public:
    explicit LauncherMouseController(bool available = false)
        : available_(available) {}
    LauncherMouseController(bool available, bool start_mouse)
        : available_(available), mouse_mode_(available && start_mouse) {}

    bool available() const { return available_; }
    bool mouse_mode() const { return mouse_mode_; }
    void UseGamepad()
    {
        mouse_mode_ = false;
        toggle_pending_ = false;
        click_guard_ = true;
        remainder_x_ = remainder_y_ = 0;
    }

    LauncherMouseDecision Update(const LauncherMouseSample& sample)
    {
        if (!available_)
            return {};
        if (!sample.enabled)
        {
            toggle_pending_ = false;
            toggle_blocked_ = sample.l3 || sample.r3;
            click_guard_ = true;
            remainder_x_ = remainder_y_ = 0;
            return {mouse_mode_};
        }
        if (toggle_blocked_)
            toggle_blocked_ = sample.l3 || sample.r3;
        else if (toggle_pending_)
        {
            if (!sample.l3 && !sample.r3)
            {
                toggle_pending_ = false;
                mouse_mode_ = !mouse_mode_;
                click_guard_ = true;
                remainder_x_ = remainder_y_ = 0;
                return {true, true};
            }
            return {true};
        }
        else if (sample.l3 && sample.r3)
        {
            toggle_pending_ = true;
            click_guard_ = true;
            remainder_x_ = remainder_y_ = 0;
            return {true};
        }
        if (!sample.zr && !sample.zl)
            click_guard_ = false;
        if (!mouse_mode_)
            return {};

        LauncherMouseDecision decision {true, false, sample.zr && !click_guard_};
        decision.right_button = sample.zl && !click_guard_;
        const double x = std::isfinite(sample.x) ? sample.x : 0;
        const double y = std::isfinite(sample.y) ? sample.y : 0;
        const double magnitude = std::hypot(x, y);
        constexpr double deadzone = 0.16;
        if (magnitude <= deadzone)
        {
            remainder_x_ = remainder_y_ = 0;
            return decision;
        }
        const double deflection = (std::min(1.0, magnitude) - deadzone) / (1.0 - deadzone);
        const double elapsed = std::isfinite(sample.elapsed_seconds)
            ? std::clamp(sample.elapsed_seconds, 0.0, 0.05) : 0;
        const double distance = 1000.0 * deflection * deflection * elapsed *
            std::clamp(sample.speed_percent, 25, 300) / 100.0;
        remainder_x_ += x / magnitude * distance;
        remainder_y_ += y / magnitude * distance;
        decision.dx = static_cast<std::int16_t>(std::trunc(remainder_x_));
        decision.dy = static_cast<std::int16_t>(std::trunc(remainder_y_));
        remainder_x_ -= decision.dx;
        remainder_y_ -= decision.dy;
        return decision;
    }

  private:
    bool available_ = false;
    bool mouse_mode_ = false;
    bool toggle_pending_ = false;
    bool toggle_blocked_ = false;
    bool click_guard_ = true;
    double remainder_x_ = 0;
    double remainder_y_ = 0;
};

// A short Minus tap must survive the stats chord's 120 ms grace window.
// Any companion button cancels Escape until Minus is released.
class MouseEscapeKey
{
  public:
    bool Update(bool minus, bool companion, bool enabled)
    {
        if (!enabled || companion) { armed_ = false; blocked_ = minus; return false; }
        if (blocked_) { blocked_ = minus; return false; }
        if (minus) { armed_ = true; return false; }
        const bool fire = armed_; armed_ = false; return fire;
    }
  private:
    bool armed_ = false, blocked_ = false;
};

class MouseButtonDelivery
{
  public:
    template <typename Send>
    void Update(bool desired, Send send)
    {
        if (desired != delivered_ && send(desired))
            delivered_ = desired;
    }
    bool held() const { return delivered_; }

  private:
    bool delivered_ = false;
};
} // namespace nxnow::input
