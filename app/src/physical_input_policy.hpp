#pragma once

#include "keyboard_input_policy.hpp"
#include "keyboard_layout.hpp"
#include <algorithm>
#include <bitset>
#include <cstdint>
#include <initializer_list>

namespace nxnow::input
{
using PhysicalKeys = std::bitset<256>;

// USB HID usages are physical positions. Send their scan code with the US
// key code of the same position; the cloud PC applies its own layout.
inline KeyboardStroke PhysicalKeyStroke(unsigned usage, KeyboardLayout layout = KeyboardLayout::Us)
{
    KeyboardStroke key {};
    if (usage >= 4 && usage <= 29)
        MapAsciiKey(static_cast<char>('a' + usage - 4), key);
    else if (usage >= 30 && usage <= 39)
        MapAsciiKey(usage == 39 ? '0' : static_cast<char>('1' + usage - 30), key);
    else if (usage >= 58 && usage <= 69)
        key.keycode = 0x70 + usage - 58;
    else if (usage >= 104 && usage <= 115)
        key.keycode = 0x7c + usage - 104;
    else if (usage >= 89 && usage <= 98)
        key.keycode = usage == 98 ? 0x60 : 0x61 + usage - 89;
    else
    {
        switch (usage)
        {
            case 40: case 88: key.keycode = 0x0d; break;
            case 41: return {0x1b, 0x01, 0};
            case 42: key.keycode = 0x08; break;
            case 43: key.keycode = 0x09; break;
            case 44: key.keycode = 0x20; break;
            case 45: MapAsciiKey('-', key); break;
            case 46: MapAsciiKey('=', key); break;
            case 47: MapAsciiKey('[', key); break;
            case 48: MapAsciiKey(']', key); break;
            case 49: case 50: MapAsciiKey('\\', key); break;
            case 51: MapAsciiKey(';', key); break;
            case 52: MapAsciiKey('\'', key); break;
            case 53: MapAsciiKey('`', key); break;
            case 54: MapAsciiKey(',', key); break;
            case 55: MapAsciiKey('.', key); break;
            case 56: MapAsciiKey('/', key); break;
            case 57: key.keycode = 0x14; break;
            case 70: key.keycode = 0x2c; break;
            case 71: key.keycode = 0x91; break;
            case 72: key.keycode = 0x13; break;
            case 73: key.keycode = 0x2d; break;
            case 74: key.keycode = 0x24; break;
            case 75: key.keycode = 0x21; break;
            case 76: key.keycode = 0x2e; break;
            case 77: key.keycode = 0x23; break;
            case 78: key.keycode = 0x22; break;
            case 79: key.keycode = 0x27; break;
            case 80: key.keycode = 0x25; break;
            case 81: key.keycode = 0x28; break;
            case 82: key.keycode = 0x26; break;
            case 83: key.keycode = 0x90; break;
            case 84: key.keycode = 0x6f; break;
            case 85: key.keycode = 0x6a; break;
            case 86: key.keycode = 0x6d; break;
            case 87: key.keycode = 0x6b; break;
            case 99: key.keycode = 0x6e; break;
            case 100: key.keycode = 0xe2; break;
            case 101: key.keycode = 0x5d; break;
            case 103: key.keycode = 0xbb; break;
            case 224: key.keycode = 0xa2; break;
            case 225: key.keycode = 0xa0; break;
            case 226: key.keycode = 0xa4; break;
            case 227: key.keycode = 0x5b; break;
            case 228: key.keycode = 0xa3; break;
            case 229: key.keycode = 0xa1; break;
            case 230: key.keycode = 0xa5; break;
            case 231: key.keycode = 0x5c; break;
        }
    }
    // GeForce NOW reads keys by position (US key codes) and applies the
    // layout requested for the cloud PC itself. Translating here as well
    // swapped A/Q, Z/W, M and punctuation twice on AZERTY/QWERTZ.
    static_cast<void>(layout);
    key.modifiers = 0;
    return key;
}

inline std::uint16_t PhysicalModifiers(const PhysicalKeys& keys)
{
    return ((keys[225] || keys[229]) ? 1 : 0) |
        ((keys[224] || keys[228]) ? 2 : 0) |
        ((keys[226] || keys[230]) ? 4 : 0) |
        ((keys[227] || keys[231]) ? 8 : 0);
}

// Some HID keyboards report modifiers only in the aggregate field.
inline void AddPhysicalModifiers(PhysicalKeys& keys, std::uint64_t hid_modifiers)
{
    if ((hid_modifiers & 1) && !keys[224] && !keys[228]) keys.set(224);
    if ((hid_modifiers & 2) && !keys[225] && !keys[229]) keys.set(225);
    if (hid_modifiers & 4) keys.set(226);
    if (hid_modifiers & 8) keys.set(230);
    if ((hid_modifiers & 16) && !keys[227] && !keys[231]) keys.set(227);
}

class PhysicalKeyboardCapture
{
  public:
    PhysicalKeys Update(const PhysicalKeys& held, bool enabled)
    {
        if (!enabled)
        {
            blocked_ |= held;
            return {};
        }
        blocked_ &= held;
        return held & ~blocked_;
    }
  private:
    PhysicalKeys blocked_;
};

// Modifier-down precedes key-down; key-up precedes modifier-up. Failed sends
// retain the delivered state and retry next frame, including all releases.
class PhysicalKeyboardDelivery
{
  public:
    explicit PhysicalKeyboardDelivery(KeyboardLayout layout = KeyboardLayout::Us) : layout_(layout) {}
    template <typename Send>
    bool Update(const PhysicalKeys& desired, Send send)
    {
        for (bool modifiers : {false, true})
            for (unsigned key = 0; key < 256; ++key)
                if (IsModifier(key) == modifiers && delivered_[key] && !desired[key])
                    if (!Deliver(key, false, send)) return false;
        for (bool modifiers : {true, false})
            for (unsigned key = 0; key < 256; ++key)
                if (IsModifier(key) == modifiers && !delivered_[key] && desired[key])
                    if (!Deliver(key, true, send)) return false;
        return true;
    }
    bool any() const { return delivered_.any(); }

  private:
    static bool IsModifier(unsigned key) { return key >= 224 && key <= 231; }
    template <typename Send>
    bool Deliver(unsigned key, bool down, Send send)
    {
        auto stroke = PhysicalKeyStroke(key, layout_);
        if (!stroke.keycode) return true;
        auto next = delivered_;
        next.set(key, down);
        stroke.modifiers = PhysicalModifiers(next);
        if (!send(stroke, down)) return false;
        delivered_ = next;
        return true;
    }
    PhysicalKeys delivered_;
    KeyboardLayout layout_;
};

class HidSampleSequence
{
  public:
    bool initialized() const { return initialized_; }
    bool Restarted(std::uint64_t newest) const { return initialized_ && newest < last_; }
    void Baseline(std::uint64_t newest) { initialized_ = true; last_ = newest; }
    bool Accept(std::uint64_t sample)
    {
        if (!initialized_ || sample <= last_) return false;
        last_ = sample;
        return true;
    }
  private:
    bool initialized_ = false;
    std::uint64_t last_ = 0;
};

class PhysicalMouseCapture
{
  public:
    unsigned Update(unsigned held, bool enabled)
    {
        if (!enabled) { blocked_ |= held; return 0; }
        blocked_ &= held;
        return held & ~blocked_ & 31;
    }
  private:
    unsigned blocked_ = 0;
};

inline int PhysicalWheelDelta(int raw)
{
    // Accept detents as well as already-normalized 120-unit wheel reports.
    const std::int64_t delta = raw;
    return static_cast<int>(std::clamp<std::int64_t>(
        delta > -120 && delta < 120 ? delta * 120 : delta, -960, 960));
}
}
