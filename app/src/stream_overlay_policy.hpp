#pragma once

#include <cstdint>

namespace nxnow::input
{

enum class OverlayChordHalf
{
    None,
    Minus,
    Plus,
};

struct OverlayChordState
{
    OverlayChordHalf pending_half = OverlayChordHalf::None;
    std::int64_t pending_since_ms = 0;
    bool disqualified             = false;
};

struct OverlayChordDecision
{
    bool toggle_overlay = false;
    bool preempt_input  = false;
    OverlayChordState next_state;
};

inline OverlayChordDecision EvaluateOverlayChord(
    bool minus_pressed,
    bool plus_pressed,
    const OverlayChordState& state,
    std::int64_t now_ms,
    std::int64_t grace_ms = 120)
{
    if (minus_pressed && plus_pressed)
    {
        if (state.disqualified)
            return {false, false, state};
        return {true, true, {}};
    }

    const OverlayChordHalf pressed_half =
        minus_pressed == plus_pressed
            ? OverlayChordHalf::None
            : (minus_pressed ? OverlayChordHalf::Minus : OverlayChordHalf::Plus);
    if (pressed_half == OverlayChordHalf::None)
        return {};

    if (state.disqualified)
    {
        OverlayChordState next = state;
        if (state.pending_half != pressed_half)
            next = {pressed_half, now_ms, true};
        return {false, false, next};
    }

    if (state.pending_half == OverlayChordHalf::None ||
        state.pending_half != pressed_half)
    {
        return {false, true, {pressed_half, now_ms, false}};
    }

    OverlayChordState next = state;
    next.disqualified = now_ms - state.pending_since_ms >= grace_ms;
    return {false, !next.disqualified, next};
}

struct StreamShortcutDecision
{
    bool toggle_stats = false;
    bool exit_stream = false;
    bool capture = false;
};

class StreamShortcutChord
{
  public:
    StreamShortcutDecision Update(bool minus, bool plus, bool zl, bool zr,
                                  bool allow_stats, std::int64_t now_ms)
    {
        // Exit always wins, including when a keyboard or diagnostics owns input.
        if (minus && plus && zl && zr)
        {
            const bool fire = !exit_latched_;
            exit_latched_ = true;
            stats_armed_ = false;
            return {false, fire, true};
        }
        if (exit_latched_)
        {
            if (!minus && !plus && !zl && !zr)
            {
                exit_latched_ = false;
                draining_ = false;
                half_ = {};
            }
            return {false, false, true};
        }
        if (!allow_stats)
        {
            stats_armed_ = false;
            half_ = {};
            draining_ = minus || plus;
            return {false, false, draining_};
        }
        if (draining_)
        {
            draining_ = minus || plus;
            return {false, false, true};
        }
        if (stats_armed_)
        {
            // Wait for both releases so adding ZL/ZR cannot toggle the HUD
            // before the four-button exit chord has been recognised.
            if (!minus && !plus)
            {
                stats_armed_ = false;
                half_ = {};
                return {true, false, true};
            }
            return {false, false, true};
        }
        const auto decision = EvaluateOverlayChord(minus, plus, half_, now_ms);
        half_ = decision.next_state;
        if (decision.toggle_overlay)
            stats_armed_ = true;
        return {false, false, decision.preempt_input};
    }

  private:
    OverlayChordState half_;
    bool stats_armed_ = false;
    bool exit_latched_ = false;
    bool draining_ = false;
};

} // namespace nxnow::input
