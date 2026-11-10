#include "queue_view.hpp"
#include "cover_image_cache.hpp"
#include "localization.hpp"
#include "ui_theme.hpp"

#include <utility>
#include <chrono>
#include <cmath>

namespace nxnow
{
namespace
{
class LoadingDotsView final : public brls::View
{
  public:
    LoadingDotsView()
    {
        setId("queue/loading-dots");
        setWidth(72);
        setHeight(20);
        setShrink(0);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style, brls::FrameContext*) override
    {
        const float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - started_).count();
        const float phase = std::fmod(elapsed, 1.2f) / 0.4f;
        const int active = static_cast<int>(phase);
        const float pulse = std::sin((phase - active) * 3.14159265f);
        nvgSave(vg);
        for (int i = 0; i < 3; ++i)
        {
            auto color = ui::Accent();
            color.a = i == active ? 0.35f + 0.65f * pulse : 0.2f;
            nvgBeginPath(vg);
            nvgCircle(vg, x + width * 0.5f + (i - 1) * 18.0f, y + height * 0.5f, 4.5f);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
        nvgRestore(vg);
    }

  private:
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
};
}

QueueView::QueueView(const QueueDisplayState& state, const StreamSettings& settings,
                     std::function<void()> cancel, std::function<void()> keyboard)
    : brls::Box(brls::Axis::COLUMN)
{
    keyboard_ = std::move(keyboard);
    setId("queue/root");
    setBackgroundColor(ui::Ground());
    setPadding(24, 40, 20, 40);

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setHeight(52);
    header->setShrink(0);
    header->setAlignItems(brls::AlignItems::CENTER);
    auto* logo = new brls::Image();
    logo->setWidth(46);
    logo->setHeight(46);
    logo->setScalingType(brls::ImageScalingType::FIT);
    logo->setImageFromRes("img/nxnow-logo-mark-small.png");
    logo->setMarginRight(12);
    header->addView(logo);
    auto* brand = ui::MakeLabel(Tr("NxNow"), 24, ui::Accent(), ui::FontRole::Heading);
    brand->setGrow(1);
    header->addView(brand);
    phase_ = ui::MakeLabel(Tr("Launching your game"), 16, ui::Muted());
    phase_->setId("queue/phase");
    header->addView(phase_);
    addView(header);

    auto* content = new brls::Box(brls::Axis::ROW);
    content->setGrow(1);
    content->setMinHeight(0);
    content->setAlignItems(brls::AlignItems::CENTER);
    content->setMarginTop(18);
    content->setMarginBottom(18);
    auto* artwork = new brls::Box(brls::Axis::COLUMN);
    artwork->setWidthPercentage(57);
    artwork->setShrink(0);
    artwork->setMarginRight(40);
    auto* image = new CachedImage();
    image->setId("queue/cover");
    image->setWidthPercentage(100);
    image->setHeight(350);
    image->setShrink(0);
    image->setCornerRadius(16);
    image->setScalingType(brls::ImageScalingType::FIT);
    image->setImageAlign(brls::ImageAlignment::CENTER);
    image->setClipsToBounds(false);
    SetCachedCoverImage(image, state.image_url);
    artwork->addView(image);
    game_ = ui::MakeLabel(state.game_title, 30, ui::Text(), ui::FontRole::Heading);
    game_->setId("queue/game");
    game_->setMarginTop(16);
    game_->setSingleLine(true);
    game_->setWidthPercentage(100);
    game_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    artwork->addView(game_);
    auto* requested = ui::MakeLabel(Tr("Requested stream") + " · " + ui::StreamSummary(settings) +
        " · " + Tr(GameLanguageLabel(settings.game_language)), 14, ui::Muted());
    requested->setId("queue/requested-settings");
    requested->setSingleLine(true);
    requested->setWidthPercentage(100);
    requested->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    artwork->addView(requested);
    content->addView(artwork);

    auto* progress = new brls::Box(brls::Axis::COLUMN);
    progress->setGrow(1);
    progress->setShrink(1);
    status_ = ui::MakeLabel({}, 26, ui::Text(), ui::FontRole::Heading);
    status_->setId("queue/status");
    status_->setSingleLine(false);
    status_->setMarginBottom(8);
    progress->addView(status_);
    detail_ = ui::MakeLabel({}, 16, ui::Muted());
    detail_->setId("queue/detail");
    detail_->setSingleLine(false);
    detail_->setMaxHeight(108);
    progress->addView(detail_);

    counter_ = new brls::Box(brls::Axis::ROW);
    counter_->setId("queue/counter");
    counter_->setHeight(90);
    counter_->setAlignItems(brls::AlignItems::CENTER);
    counter_->setMarginTop(12);
    counter_->setPadding(10, 18, 10, 18);
    counter_->setCornerRadius(12);
    counter_->setBackgroundColor(ui::Raised());
    position_heading_ = ui::MakeLabel(Tr("Position in queue"), 16, ui::Muted());
    position_heading_->setId("queue/position-heading");
    position_heading_->setGrow(1);
    position_heading_->setSingleLine(false);
    counter_->addView(position_heading_);
    position_ = ui::MakeLabel({}, 48, ui::Accent(), ui::FontRole::Display);
    position_->setId("queue/position");
    position_->setMarginLeft(12);
    position_->setSingleLine(true);
    counter_->addView(position_);
    progress->addView(counter_);

    const char* names[] = {"Account", "Queue", "Game setup", "Connection", "Video"};
    for (size_t i = 0; i < steps_.size(); ++i)
    {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(38);
        row->setMarginTop(i == 0 ? 16 : 0);
        row->setAlignItems(brls::AlignItems::CENTER);
        step_numbers_[i] = ui::MakeLabel(std::to_string(i + 1), 16, ui::Muted(), ui::FontRole::Mono);
        step_numbers_[i]->setWidth(32);
        row->addView(step_numbers_[i]);
        steps_[i] = ui::MakeLabel(Tr(names[i]), 17, ui::Muted());
        steps_[i]->setId("queue/step/" + std::to_string(i));
        row->addView(steps_[i]);
        progress->addView(row);
    }
    content->addView(progress);
    addView(content);

    auto* loading = new brls::Box(brls::Axis::ROW);
    loading->setHeight(20);
    loading->setShrink(0);
    loading->setMarginBottom(8);
    loading->setJustifyContent(brls::JustifyContent::CENTER);
    loading_dots_ = new LoadingDotsView();
    loading->addView(loading_dots_);
    addView(loading);

    auto* footer = new brls::Box(brls::Axis::ROW);
    footer->setHeight(52);
    footer->setShrink(0);
    footer->setAlignItems(brls::AlignItems::CENTER);
    if (cancel)
    {
        auto* space = new brls::Box();
        space->setGrow(1);
        footer->addView(space);
        cancel_ = new ui::ActionRow(Tr("Cancel launch"), {},
            [cancel](brls::View*) { cancel(); return true; }, ui::ActionTone::Outlined);
        cancel_->setId("queue/cancel");
        cancel_->setWidth(300);
        cancel_->setHeight(48);
        cancel_->SetCentered(true);
        footer->addView(cancel_);
        registerAction(Tr("Cancel launch"), brls::BUTTON_B,
            [cancel](brls::View*) { cancel(); return true; }, true);
        // Start on the progress indicator so confirming does not cancel by accident.
        status_->setFocusable(true);
        status_->setHideHighlight(true);
        for (auto direction : {brls::FocusDirection::UP, brls::FocusDirection::DOWN,
                               brls::FocusDirection::LEFT, brls::FocusDirection::RIGHT})
            status_->setCustomNavigationRoute(direction, cancel_);
        cancel_->setCustomNavigationRoute(brls::FocusDirection::UP, status_);
        setLastFocusedView(status_);
    }
    else
    {
        cancel_ = nullptr;
        footer->addView(ui::MakeLabel(Tr("[+] + [-] + ZL + ZR : return to NxNow"), 16, ui::Muted()));
    }
    addView(footer);
    Update(state);
}

void QueueView::draw(NVGcontext* vg, float x, float y, float width, float height,
                     brls::Style style, brls::FrameContext* ctx)
{
    if (keyboard_)
    {
        brls::ControllerState state;
        brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&state);
        const bool focused = brls::Application::getCurrentFocus() &&
            brls::Application::getCurrentFocus()->getParentActivity() == getParentActivity();
        if (keyboard_chord_.Update(state.buttons[brls::BUTTON_RSB],
                state.buttons[brls::BUTTON_RT], focused).open)
            keyboard_();
    }
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void QueueView::Update(const QueueDisplayState& state)
{
    if (initialized_ && state == displayed_)
        return;
    initialized_ = true;
    displayed_ = state;
    loading_dots_->setVisibility(state.failed ? brls::Visibility::INVISIBLE : brls::Visibility::VISIBLE);
    game_->setText(state.game_title);
    status_->setText(Tr(state.status));
    status_->setTextColor(state.failed ? ui::Danger() : ui::Text());
    detail_->setText(Tr(state.detail));
    const bool queued = state.stage == 1 && !state.failed;
    counter_->setVisibility(queued ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    position_->setText(state.position < 0 ? "—" : std::to_string(state.position));
    position_heading_->setText(Tr(state.position < 0 ? "Waiting for your queue position" :
        state.position == 0 ? "You are next" : "Position in queue"));
    if (cancel_)
    {
        cancel_->SetTitle(Tr(state.failed ? "Close" : "Cancel launch"));
    }
    for (size_t i = 0; i < steps_.size(); ++i)
    {
        const bool complete = !state.failed && static_cast<int>(i) < state.stage;
        const bool active = !state.failed && static_cast<int>(i) == state.stage;
        steps_[i]->setTextColor(active ? ui::Text() : complete ? ui::Accent() : ui::Muted());
        step_numbers_[i]->setText(complete ? "✓" : active ? "›" : std::to_string(i + 1));
        step_numbers_[i]->setTextColor(active || complete ? ui::Accent() : ui::Muted());
    }
}
}
