#pragma once

namespace nxnow::input
{
struct WowChatDecision
{
    bool open = false;
    bool capture = false;
    bool tap_r3 = false;
};

// Capture R3 until release. Either order of ZR + R3 opens the keyboard
// once both buttons are released. ZR alone stays immediate for gameplay.
class WowChatChord
{
  public:
    WowChatDecision Update(bool r3, bool zr, bool enabled)
    {
        if (!enabled)
        {
            pending_ = combined_ = false;
            blocked_ = r3 || zr;
            zr_was_down_ = zr;
            return {};
        }
        if (blocked_)
        {
            if (!r3 && !zr) blocked_ = false;
            zr_was_down_ = zr;
            return {};
        }
        if (pending_)
        {
            if (r3 && zr && !zr_was_down_) combined_ = true;
            zr_was_down_ = zr;
            if (!r3 && !zr)
            {
                const auto result = WowChatDecision{combined_, true, !combined_};
                pending_ = combined_ = false;
                return result;
            }
            return {false, true};
        }
        if (r3)
        {
            pending_ = true;
            combined_ = zr;
            zr_was_down_ = zr;
            return {false, true};
        }
        zr_was_down_ = zr;
        return {};
    }
  private:
    bool pending_ = false;
    bool combined_ = false;
    bool blocked_ = false;
    bool zr_was_down_ = false;
};
}
