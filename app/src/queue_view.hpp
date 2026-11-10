#pragma once

#include "stream_settings.hpp"
#include "wow_chat_chord.hpp"
#include <borealis.hpp>
#include <array>
#include <functional>
#include <string>

namespace nxnow
{

namespace ui { class ActionRow; }

struct QueueDisplayState
{
    std::string game_title;
    std::string status;
    std::string detail;
    int position = -1;
    int stage = 0;
    bool failed = false;
    std::string image_url;

    bool operator==(const QueueDisplayState&) const = default;
};

class QueueView final : public brls::Box
{
  public:
    QueueView(const QueueDisplayState& state, const StreamSettings& settings,
              std::function<void()> cancel = {}, std::function<void()> keyboard = {});
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
    void Update(const QueueDisplayState& state);

  private:
    brls::Label* game_;
    brls::Label* status_;
    brls::Label* position_;
    brls::Label* position_heading_;
    brls::Box* counter_;
    brls::Label* detail_;
    brls::Label* phase_;
    brls::View* loading_dots_;
    ui::ActionRow* cancel_;
    std::array<brls::Label*, 5> steps_ {};
    std::array<brls::Label*, 5> step_numbers_ {};
    QueueDisplayState displayed_;
    bool initialized_ = false;
    std::function<void()> keyboard_;
    input::WowChatChord keyboard_chord_;
};

}
