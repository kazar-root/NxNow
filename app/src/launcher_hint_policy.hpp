#pragma once
#include <chrono>
namespace nxnow::input
{
class LauncherHintWindow
{
  public:
    using Clock = std::chrono::steady_clock;
    void FirstFrame(Clock::time_point now)
    { if (!started_) { started_ = true; until_ = now + std::chrono::seconds(5); } }
    void ModeChanged(Clock::time_point now) { until_ = now + std::chrono::seconds(2); }
    bool Visible(Clock::time_point now) const { return now < until_; }
  private:
    bool started_ = false;
    Clock::time_point until_ {};
};
}
