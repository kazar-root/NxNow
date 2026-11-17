#include "queue_view.hpp"
#include "cover_image_cache.hpp"
#include "localization.hpp"
#include "ui_theme.hpp"

#include <algorithm>
#include <utility>
#include <chrono>
#include <cmath>

namespace nxnow
{
namespace
{
class LaunchTitleLabel final : public brls::Label
{
  public:
    LaunchTitleLabel()
    {
        font = ui::Font(ui::FontRole::Heading);
        setFontSize(30);
        setHeight(80);
        setShrink(0);
        setSingleLine(false);
        setAutoAnimate(false);
        setHorizontalAlign(brls::HorizontalAlign::CENTER);
    }

    // The title owns a fixed two-line slot. Do not let the stock label's
    // single-line truncation replace the measured lines rendered below.
    void onLayout() override {}

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style, brls::FrameContext*) override
    {
        if (width <= 16 || height <= 0) return;
        nvgSave(vg);
        nvgFontFaceId(vg, font);
        nvgFontQuality(vg, fontQuality);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        if (cached_text_ != fullText || cached_width_ != width)
            PrepareLines(vg, width);
        nvgFontSize(vg, rendered_font_size_);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, a(ui::Text()));
        nvgIntersectScissor(vg, x, y, width, height);
        const float line_height = rendered_font_size_ * 1.2f;
        const float top = y + (height - line_count_ * line_height) / 2;
        for (int i = 0; i < line_count_; ++i)
            nvgText(vg, x + width / 2, top + i * line_height, lines_[i].c_str(), nullptr);
        nvgRestore(vg);
    }

  private:
    static void RemoveLastCodePoint(std::string& text)
    {
        size_t end = text.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
            --end;
        text.resize(end);
    }

    void PrepareLines(NVGcontext* vg, float width)
    {
        const float available = width - 16;
        NVGtextRow rows[3];
        int count = 0;
        for (float size : {30.0f, 28.0f, 26.0f, 24.0f})
        {
            rendered_font_size_ = size;
            nvgFontSize(vg, size);
            count = nvgTextBreakLines(vg, fullText.c_str(), nullptr, available, rows, 3);
            if (count <= 2) break;
        }
        line_count_ = std::min(count, 2);
        for (int i = 0; i < line_count_; ++i)
        {
            auto& line = lines_[i];
            line.assign(rows[i].start, rows[i].end);
            const auto last = line.find_last_not_of(" \t\r\n");
            line.resize(last == std::string::npos ? 0 : last + 1);
            const auto fits = [&](const std::string& value) {
                float bounds[4];
                const float advance = nvgTextBounds(vg, 0, 0, value.c_str(), nullptr, bounds);
                return std::max(advance, bounds[2]) - std::min(0.0f, bounds[0]) <= available;
            };
            // Unusually long names still stay within two lines. Keep UTF-8
            // characters intact when the final line needs an ellipsis.
            if ((i == 1 && count > 2) || !fits(line))
            {
                while (!line.empty() && !fits(line + "…")) RemoveLastCodePoint(line);
                line += "…";
            }
        }
        cached_text_ = fullText;
        cached_width_ = width;
    }

    std::array<std::string, 2> lines_;
    std::string cached_text_;
    float cached_width_ = -1;
    float rendered_font_size_ = 30;
    int line_count_ = 0;
};

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
    logo->setId("queue/logo");
    logo->setWidth(46);
    logo->setHeight(46);
    logo->setScalingType(brls::ImageScalingType::FIT);
    logo->setImageFromRes("img/nxnow-logo-external-small.png");
    logo->setMarginRight(12);
    header->addView(logo);
    auto* brand = ui::MakeLabel(Tr("NxNow"), 24, ui::Text(), ui::FontRole::Heading);
    brand->setId("queue/brand");
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
    artwork->setMaxHeightPercentage(100);
    artwork->setMinHeight(0);
    artwork->setShrink(0);
    artwork->setMarginRight(40);
    auto* image = new CachedImage();
    image->setId("queue/cover");
    image->setWidthPercentage(100);
    image->setHeight(350);
    image->setShrink(1);
    image->setMinHeight(0);
    image->setCornerRadius(16);
    image->setScalingType(brls::ImageScalingType::FIT);
    image->setImageAlign(brls::ImageAlignment::CENTER);
    image->setClipsToBounds(false);
    SetCachedCoverImage(image, state.image_url);
    artwork->addView(image);
    game_ = new LaunchTitleLabel();
    game_->setText(state.game_title);
    game_->setId("queue/game");
    game_->setMarginTop(16);
    game_->setWidthPercentage(100);
    game_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    artwork->addView(game_);
    auto* requested = ui::MakeLabel(Tr("Requested stream") + " · " + ui::StreamSummary(settings) +
        " · " + Tr(GameLanguageLabel(settings.game_language)), 14, ui::Muted());
    requested->setId("queue/requested-settings");
    requested->setMarginTop(8);
    requested->setSingleLine(true);
    requested->setShrink(0);
    requested->setWidthPercentage(100);
    requested->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    artwork->addView(requested);
    content->addView(artwork);

    auto* progress = new brls::Box(brls::Axis::COLUMN);
    progress->setGrow(1);
    progress->setShrink(1);
    progress->setMinWidth(0);
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
