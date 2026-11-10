#pragma once
#include <cstdint>

namespace nxnow::input
{
struct IntroShortcutDecision
{
    bool fire = false;
    bool capture = false;
    bool tap_start = false;
};

class BnetIntroShortcut
{
  public:
    IntroShortcutDecision Update(bool plus, bool other_button, bool enabled, std::int64_t now_ms)
    {
        if (!enabled || other_button)
        {
            pending_ = false;
            blocked_ = plus;
            return {};
        }
        if (!plus)
        {
            const bool capture = pending_ || blocked_;
            const bool fire = pending_ && now_ms - started_ms_ >= 1000;
            const bool tap = pending_ && !fire;
            pending_ = blocked_ = false;
            return {fire, capture, tap};
        }
        if (blocked_)
            return {false, true};
        if (!pending_)
        {
            pending_ = true;
            started_ms_ = now_ms;
        }
        if (now_ms - started_ms_ >= 1000)
        {
            pending_ = false;
            blocked_ = true;
            return {true, true};
        }
        return {false, true};
    }

  private:
    bool pending_ = false;
    bool blocked_ = false;
    std::int64_t started_ms_ = 0;
};
}
