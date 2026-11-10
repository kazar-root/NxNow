#pragma once

#include <cstdint>
#include <string>

namespace nxnow
{

constexpr std::uint16_t kXboxButtonA = 0x1000;
constexpr std::uint16_t kXboxButtonB = 0x2000;
constexpr std::uint16_t kXboxButtonX = 0x4000;
constexpr std::uint16_t kXboxButtonY = 0x8000;

struct ShoulderInputs
{
    std::uint16_t buttons;
    std::uint8_t left_trigger;
    std::uint8_t right_trigger;
};

// Switch triggers are digital: send the full Xbox trigger range. They are
// independent of the ABXY layout and must never be swapped with L/R.
constexpr ShoulderInputs MapShoulderInputs(bool l, bool r, bool zl, bool zr)
{
    return {
        static_cast<std::uint16_t>((l ? 0x0100 : 0) | (r ? 0x0200 : 0)),
        static_cast<std::uint8_t>(zl ? 0xff : 0),
        static_cast<std::uint8_t>(zr ? 0xff : 0)
    };
}

inline std::uint16_t MapFaceButtons(
    const std::string& layout,
    bool switch_a,
    bool switch_b,
    bool switch_x,
    bool switch_y)
{
    std::uint16_t buttons = 0;
    const bool switch_labels = layout != "Xbox";

    if (switch_a)
        buttons |= switch_labels ? kXboxButtonA : kXboxButtonB;
    if (switch_b)
        buttons |= switch_labels ? kXboxButtonB : kXboxButtonA;
    if (switch_x)
        buttons |= switch_labels ? kXboxButtonX : kXboxButtonY;
    if (switch_y)
        buttons |= switch_labels ? kXboxButtonY : kXboxButtonX;

    return buttons;
}

} // namespace nxnow
