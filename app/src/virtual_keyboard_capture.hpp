#pragma once
#include <chrono>
namespace nxnow::input
{
// The native keyboard owns controller navigation while it is visible and
// until submitted text and held buttons have drained. This is not a mode change.
class VirtualKeyboardCapture
{
  public:
    using Clock = std::chrono::steady_clock;
    void Begin(Clock::time_point now) { active_ = true; Quiet(now); }
    void Quiet(Clock::time_point now) { until_ = now + std::chrono::milliseconds(150); }
    bool Update(bool visible, bool held, bool pending_text, Clock::time_point now)
    {
        if (!active_) return false;
        if (visible || held || pending_text) { Quiet(now); return true; }
        if (now < until_) return true;
        active_ = false; return false;
    }
    bool active() const { return active_; }
  private:
    bool active_ = false;
    Clock::time_point until_ {};
};
}
