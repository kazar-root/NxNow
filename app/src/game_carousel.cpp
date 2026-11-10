#include "game_carousel.hpp"
#include "carousel_policy.hpp"
#include "localization.hpp"
#include <borealis/core/touch/tap_gesture.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nxnow
{
namespace { constexpr float kStep = 222, kAnchor = 278, kWidth = 196, kHeight = 286; }
GameCarouselView::GameCarouselView(std::function<void(const std::string&)> select,
                                   std::function<void()> activate, std::function<void()> next_page,
                                   std::function<void()> previous_page)
    : select_(std::move(select)), activate_(std::move(activate)), next_page_(std::move(next_page)),
      previous_page_(std::move(previous_page))
{
    setId("library-carousel"); setHeight(334); setShrink(0);
    setFocusable(true); setHideHighlight(true); setHideClickAnimation(true);
    registerAction({}, brls::BUTTON_NAV_LEFT, [this](brls::View*) { MoveSelection(-1); return true; }, true, true);
    registerAction({}, brls::BUTTON_NAV_RIGHT, [this](brls::View*) { MoveSelection(1); return true; }, true, true);
    registerClickAction([this](brls::View*) { ActivateSelected(); return true; });
    updateActionHint(brls::BUTTON_A, Tr("View game"));
    addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound*) {
        if (status.state != brls::GestureState::END || items_.empty()) return;
        const float relative = (status.position.x - getFrame().getMinX() - kAnchor) / kStep;
        const auto target = std::llround(position_.getValue() + relative) + has_previous_page_;
        if (target < 0 || target >= static_cast<long long>(items_.size() + has_next_page_ + has_previous_page_)) return;
        brls::Application::giveFocus(this);
        if (static_cast<size_t>(target) == selected_) ActivateSelected();
        else Select(static_cast<size_t>(target), true);
    }));
}
std::string GameCarouselView::SelectedIdentity() const
{ return items_.empty() || IsNextPageSelected() || IsPreviousPageSelected() ? std::string {} : items_[SelectedImageIndex()].id; }
std::size_t GameCarouselView::SelectedImageIndex() const
{
    if (items_.empty() || IsPreviousPageSelected()) return 0;
    return std::min(selected_ - has_previous_page_, items_.size() - 1);
}
void GameCarouselView::ActivateSelected()
{
    if (IsPreviousPageSelected()) { if (previous_page_) previous_page_(); }
    else if (IsNextPageSelected()) { if (next_page_) next_page_(); }
    else if (!items_.empty()) activate_();
}
void GameCarouselView::SetGames(std::vector<CarouselItem> games, const std::string& selected_id,
                               bool has_next_page, bool has_previous_page)
{
    if (games.size() > library::kCarouselTextureLimit)
        throw std::logic_error("Carousel accepts one page of at most ten games");
    items_ = std::move(games);
    has_next_page_ = has_next_page && !items_.empty() && static_cast<bool>(next_page_);
    has_previous_page_ = has_previous_page && !items_.empty() && static_cast<bool>(previous_page_);
    updateActionHint(brls::BUTTON_A, Tr("View game"));
    selected_ = has_previous_page_;
    for (size_t i = 0; i < items_.size(); ++i) if (items_[i].id == selected_id) selected_ = i + has_previous_page_;
    setFocusable(!items_.empty());
    previous_selected_ = selected_; selection_mix_ = 1;
    position_ = static_cast<float>(selected_) - has_previous_page_;
    ReconcileTextures(true);
    if (!items_.empty()) select_(items_[SelectedImageIndex()].id);
    ReconcileTextures();
}
bool GameCarouselView::PollArtwork()
{
    bool finished = true;
    for (auto& slot : slots_)
    {
        slot.image->Poll();
        finished = slot.image->HasFinished() && finished;
    }
    return finished;
}
void GameCarouselView::MoveSelection(int delta)
{
    if (!items_.empty()) Select(library::CarouselTarget(selected_, delta, items_.size() + has_next_page_ + has_previous_page_), true);
}
void GameCarouselView::Select(size_t index, bool animate)
{
    if (index >= items_.size() + has_next_page_ + has_previous_page_ || index == selected_) return;
    previous_selected_ = selected_; selected_ = index;
    selection_mix_ = 0;
    selection_mix_.addStep(1, 180, brls::EasingFunction::cubicOut); selection_mix_.start();
    const float target_position = static_cast<float>(index) - has_previous_page_;
    if (animate)
    {
        const float previous = position_.getValue();
        if (std::abs(target_position - previous) > 2)
            position_ = target_position + (target_position > previous ? -1.0f : 1.0f);
        position_.reset();
        position_.addStep(target_position, 220, brls::EasingFunction::cubicOut);
        position_.start();
    }
    else position_ = target_position;
    ReconcileTextures(true);
    updateActionHint(brls::BUTTON_A, Tr(IsPreviousPageSelected() ? "Previous page" : IsNextPageSelected() ? "Next page" : "View game"));
    if (!IsNextPageSelected() && !IsPreviousPageSelected()) select_(items_[SelectedImageIndex()].id);
    ReconcileTextures();
}
void GameCarouselView::ReconcileTextures(bool selected_only)
{
    const auto [first, last] = library::CarouselWindow(SelectedImageIndex(), items_.size());
    std::erase_if(slots_, [&](const Slot& slot) {
        return std::none_of(items_.begin() + first, items_.begin() + last,
            [&](const auto& item) { return item.id == slot.id && item.image_url == slot.url; });
    });
    std::vector<size_t> priority;
    const auto image_selection = SelectedImageIndex();
    if (!items_.empty()) priority.push_back(image_selection);
    if (!selected_only)
        for (size_t distance = 1; distance < library::kCarouselTextureLimit; ++distance)
        {
            if (image_selection + distance < last) priority.push_back(image_selection + distance);
            if (image_selection >= distance && image_selection - distance >= first) priority.push_back(image_selection - distance);
        }
    for (const size_t i : priority)
    {
        const auto& item = items_[i];
        if (std::any_of(slots_.begin(), slots_.end(), [&](const Slot& slot) { return slot.id == item.id; })) continue;
        auto image = std::make_unique<CachedImage>(); image->SetArtwork(item.image_url);
        slots_.push_back({item.id, item.image_url, std::move(image)});
    }
}
void GameCarouselView::draw(NVGcontext* vg, float x, float y, float width, float height,
                             brls::Style, brls::FrameContext*)
{
    nvgSave(vg); nvgGlobalAlpha(vg, a(ui::Text()).a); nvgIntersectScissor(vg, x, y, width, height);
    const float position = position_.getValue();
    const auto [first, last] = library::CarouselWindow(SelectedImageIndex(), items_.size());
    for (int i = static_cast<int>(first) - has_previous_page_; i < static_cast<int>(last + has_next_page_); ++i)
    {
        const float mix = selection_mix_.getValue();
        const auto slot = static_cast<std::size_t>(i + has_previous_page_);
        const float emphasis = slot == selected_ ? mix : slot == previous_selected_ ? 1 - mix : 0;
        const float scale = 1 + 0.05f * emphasis;
        const float w = kWidth * scale, h = kHeight * scale;
        const float left = x + kAnchor + (static_cast<float>(i) - position) * kStep - w / 2;
        const float top = y + (height - h) / 2;
        if (left + w < x - 15 || left > x + width + 15) continue;
        const bool previous_card = i < 0;
        const bool page_card = previous_card || i == static_cast<int>(items_.size());
        const auto found = page_card ? slots_.end() : std::find_if(slots_.begin(), slots_.end(), [&](const Slot& slot) { return slot.id == items_[i].id; });
        ui::DrawHalo(vg, left, top, w, h, 15, emphasis);
        nvgBeginPath(vg); nvgRoundedRect(vg, left + 2, top + 9, w, h, 15);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 85)); nvgFill(vg);
        nvgBeginPath(vg); nvgRoundedRect(vg, left, top, w, h, 15);
        nvgFillColor(vg, ui::Raised()); nvgFill(vg);
        if (page_card)
        {
            const float cx = left + w / 2, cy = top + h * .42f;
            const float direction = previous_card ? -1 : 1;
            nvgBeginPath(vg); nvgMoveTo(vg, cx - 28 * direction, cy); nvgLineTo(vg, cx + 28 * direction, cy);
            nvgMoveTo(vg, cx + 9 * direction, cy - 19); nvgLineTo(vg, cx + 28 * direction, cy); nvgLineTo(vg, cx + 9 * direction, cy + 19);
            nvgStrokeWidth(vg, 4); nvgLineCap(vg, NVG_ROUND); nvgLineJoin(vg, NVG_ROUND);
            nvgStrokeColor(vg, ui::Accent()); nvgStroke(vg);
            const auto caption = Tr(previous_card ? "Previous page" : "Next page");
            nvgFontFaceId(vg, ui::Font(ui::FontRole::Heading));
            nvgFontSize(vg, ui::TextWidth(caption, 18, ui::FontRole::Heading) > w - 24 ? 14 : 18);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE); nvgFillColor(vg, ui::Text());
            nvgText(vg, cx, top + h * .68f, caption.c_str(), nullptr);
            nvgFontFaceId(vg, ui::Font(ui::FontRole::Body)); nvgFontSize(vg, 15); nvgFillColor(vg, ui::Muted());
            nvgText(vg, cx, top + h * .80f, Tr("More games").c_str(), nullptr);
        }
        else if (found != slots_.end())
            ui::DrawArtwork(vg, *found->image, left, top, w, h, false, 1, 14);
        if (!page_card && (found == slots_.end() || !found->image->IsReady()))
        {
            ui::DrawIcon(vg, ui::ConsoleIcon::Library, left + w / 2 - 18, top + h / 2 - 18, 36, ui::Muted());
            // The real title is visible below while artwork is loading.
        }
        nvgBeginPath(vg); nvgRoundedRect(vg, left - 1.5f, top - 1.5f, w + 3, h + 3, 16);
        nvgStrokeWidth(vg, 1 + 1.5f * emphasis);
        nvgStrokeColor(vg, emphasis > 0.01f ? ui::Opacity(ui::Accent(), emphasis) : ui::Opacity(ui::Text(), 0.12f));
        nvgStroke(vg);
    }
    nvgRestore(vg);
}
}
