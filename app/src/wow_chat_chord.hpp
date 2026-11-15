#pragma once

namespace nxnow::input
{
struct WowChatDecision
{
    bool open = false;
    bool capture = false;
    bool tap_r3 = false;
};

// The WoW chat keyboard presses Enter to open the chat box. That would submit
// the Battle.net sign-in form, which is filled in mouse mode, so mouse mode
// gets the plain keyboard and gamepad mode keeps the in-game chat keyboard.
inline bool UseWowChatKeyboard(bool wow_session, bool mouse_mode)
{
    return wow_session && !mouse_mode;
}

// While the keyboard is open, the same R3 + ZR chord closes it once both
// buttons are released. Buttons still held when it opened are ignored.
class KeyboardCloseChord
{
  public:
    bool Update(bool r3, bool zr, bool keyboard_visible)
    {
        if (!keyboard_visible)
        {
            armed_ = false;
            blocked_ = r3 || zr;
            return false;
        }
        if (blocked_)
        {
            if (!r3 && !zr) blocked_ = false;
            return false;
        }
        if (r3 && zr) armed_ = true;
        if (armed_ && !r3 && !zr)
        {
            armed_ = false;
            return true;
        }
        return false;
    }

  private:
    bool armed_ = false;
    bool blocked_ = false;
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
