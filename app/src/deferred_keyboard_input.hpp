#pragma once
#include "localized_keyboard_policy.hpp"
#include <deque>
#include <string_view>

namespace nxnow::input
{
class DeferredKeyboardInput
{
  public:
    static constexpr size_t kMaxEvents = 16384;
    bool Append(KeyboardStroke stroke)
    {
        const auto tap = MakeKeyboardTap(stroke);
        if (events_.size() + tap.size > kMaxEvents) return false;
        for (size_t i = 0; i < tap.size; ++i) events_.push_back(tap.events[i]);
        return true;
    }
    bool AppendCharacter(char32_t c, KeyboardLayout layout)
    {
        TextKeySequence mapped;
        if (!MapTextCharacter(c, layout, mapped) || events_.size() + mapped.size * 10 > kMaxEvents) return false;
        for (size_t i = 0; i < mapped.size; ++i) Append(mapped.strokes[i]);
        return true;
    }
    bool AppendEdit(std::string_view before, std::string_view after, KeyboardLayout layout)
    {
        std::vector<char32_t> old_text, new_text;
        if (!DecodeRemoteText(before, layout, old_text) || !DecodeRemoteText(after, layout, new_text)) return false;
        size_t prefix = 0;
        while (prefix < old_text.size() && prefix < new_text.size() && old_text[prefix] == new_text[prefix]) ++prefix;
        if (events_.size() + (old_text.size() - prefix) * 2 + (new_text.size() - prefix) * 20 > kMaxEvents) return false;
        for (size_t i = prefix; i < old_text.size(); ++i) Append({0x08, 0x0e, 0});
        for (size_t i = prefix; i < new_text.size(); ++i) AppendCharacter(new_text[i], layout);
        return true;
    }
    bool AppendText(std::string_view text, KeyboardLayout layout = KeyboardLayout::Us)
    {
        std::vector<char32_t> decoded;
        if (!DecodeRemoteText(text, layout, decoded) || events_.size() + 2 + decoded.size() * 20 > kMaxEvents ||
            !AppendEdit({}, text, layout)) return false;
        return Append({0x0d, 0x1c, 0});
    }
    template <typename Send> void Drain(bool ready, Send send)
    {
        if (!ready) return;
        for (int i = 0; i < 24 && !events_.empty(); ++i)
        {
            if (!send(events_.front())) break;
            events_.pop_front();
        }
    }
    size_t size() const { return events_.size(); }
    void Clear() { for (auto& event : events_) event = {}; events_.clear(); }
    ~DeferredKeyboardInput() { Clear(); }
  private:
    std::deque<KeyboardEvent> events_;
};
}
