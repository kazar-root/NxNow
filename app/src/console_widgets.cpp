#include "console_widgets.hpp"
#include "ui_image_lifecycle.hpp"
#include <borealis/core/touch/tap_gesture.hpp>
#include <algorithm>
#include <cmath>

namespace nxnow::ui
{
NVGcolor Opacity(NVGcolor color, float alpha) { color.a *= alpha; return color; }

void DrawIcon(NVGcontext* vg, ConsoleIcon icon, float x, float y, float size, NVGcolor color)
{
    nvgSave(vg);
    nvgTranslate(vg, x, y);
    nvgScale(vg, size / 24, size / 24);
    nvgStrokeWidth(vg, 1.8f);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStrokeColor(vg, color);
    nvgFillColor(vg, color);
    nvgBeginPath(vg);
    switch (icon)
    {
        case ConsoleIcon::Menu:
            for (float row : {6, 12, 18}) { nvgMoveTo(vg, 4, row); nvgLineTo(vg, 20, row); }
            break;
        case ConsoleIcon::Favorite:
            for (int i = 0; i < 10; ++i)
            {
                const float a = -1.5707963f + i * 0.6283185f, r = i % 2 ? 4.8f : 10;
                const float px = 12 + std::cos(a) * r, py = 12 + std::sin(a) * r;
                if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
            }
            nvgClosePath(vg); break;
        case ConsoleIcon::Library:
            nvgRoundedRect(vg, 3, 5, 5, 15, 1.5f);
            nvgRoundedRect(vg, 10, 5, 5, 15, 1.5f);
            nvgMoveTo(vg, 18, 4); nvgLineTo(vg, 22, 19); break;
        case ConsoleIcon::Catalog:
            nvgMoveTo(vg, 6, 9);
            nvgBezierTo(vg, 4, 9, 3, 10, 2.5f, 12.5f);
            nvgLineTo(vg, 1.8f, 18.5f);
            nvgBezierTo(vg, 1.5f, 21, 3.3f, 22, 5, 20.5f);
            nvgLineTo(vg, 7, 18.5f); nvgLineTo(vg, 14, 18.5f);
            nvgLineTo(vg, 16, 20.5f);
            nvgBezierTo(vg, 17.7f, 22, 19.5f, 21, 19.2f, 18.5f);
            nvgLineTo(vg, 18.5f, 12.5f);
            nvgBezierTo(vg, 18, 10, 17, 9, 15, 9);
            nvgClosePath(vg);
            nvgMoveTo(vg, 5, 13.5f); nvgLineTo(vg, 9, 13.5f);
            nvgMoveTo(vg, 7, 11.5f); nvgLineTo(vg, 7, 15.5f);
            nvgCircle(vg, 14, 12.5f, 0.7f); nvgCircle(vg, 16, 15, 0.7f);
            nvgMoveTo(vg, 19, 2); nvgLineTo(vg, 19, 7);
            nvgMoveTo(vg, 16.5f, 4.5f); nvgLineTo(vg, 21.5f, 4.5f);
            break;
        case ConsoleIcon::Settings:
            nvgCircle(vg, 12, 12, 6.8f);
            for (int i = 0; i < 8; ++i)
            {
                const float a = i * 0.785398f;
                nvgMoveTo(vg, 12 + std::cos(a) * 7, 12 + std::sin(a) * 7);
                nvgLineTo(vg, 12 + std::cos(a) * 10, 12 + std::sin(a) * 10);
            }
            nvgCircle(vg, 12, 12, 2.6f); break;
        case ConsoleIcon::Account:
            nvgCircle(vg, 12, 8, 4);
            nvgMoveTo(vg, 4, 21); nvgBezierTo(vg, 4, 11, 20, 11, 20, 21); break;
        case ConsoleIcon::Info:
            nvgCircle(vg, 12, 12, 9);
            nvgMoveTo(vg, 12, 11); nvgLineTo(vg, 12, 17);
            nvgMoveTo(vg, 12, 7); nvgLineTo(vg, 12, 7.1f); break;
        case ConsoleIcon::Search:
            nvgCircle(vg, 10, 10, 6.5f);
            nvgMoveTo(vg, 15, 15); nvgLineTo(vg, 21, 21); break;
        case ConsoleIcon::Play:
            nvgMoveTo(vg, 7, 4); nvgLineTo(vg, 20, 12); nvgLineTo(vg, 7, 20);
            nvgClosePath(vg); nvgFill(vg); nvgRestore(vg); return;
    }
    nvgStroke(vg);
    nvgRestore(vg);
}

void DrawHalo(NVGcontext* vg, float x, float y, float w, float h, float r, float amount)
{
    if (amount <= 0) return;
    // Two cheap translucent strokes, no offscreen buffer or blur pass.
    for (int i = 2; i >= 1; --i)
    {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x - i * 2, y - i * 2, w + i * 4, h + i * 4, r + i * 2);
        nvgStrokeWidth(vg, 3 + i * 2);
        nvgStrokeColor(vg, Opacity(Accent(), amount * (i == 1 ? 0.12f : 0.045f)));
        nvgStroke(vg);
    }
}

void DrawArtwork(NVGcontext* vg, CachedImage& image, float x, float y, float w, float h,
                 bool fill, float opacity, float radius)
{
    image.Poll();
    if (!image.IsReady() || !image.getTexture() || w <= 0 || h <= 0) return;
    const float iw = image.getOriginalImageWidth(), ih = image.getOriginalImageHeight();
    if (iw <= 0 || ih <= 0 || !std::isfinite(iw) || !std::isfinite(ih)) return;
    const float scale = fill ? std::max(w / iw, h / ih) : std::min(w / iw, h / ih);
    const float dx = x + (w - iw * scale) / 2, dy = y + (h - ih * scale) / 2;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, fill ? x : dx, fill ? y : dy,
        fill ? w : iw * scale, fill ? h : ih * scale, radius);
    nvgFillPaint(vg, nvgImagePattern(vg, dx, dy, iw * scale, ih * scale, 0,
        image.getTexture(), opacity));
    nvgFill(vg);
}

ConsoleButton::ConsoleButton(ConsoleIcon icon, std::string title, std::function<void()> action, bool primary)
    : brls::Box(brls::Axis::ROW), icon_(icon), title_(std::move(title)), primary_(primary)
{
    setWidth(primary ? 232 : 54);
    setHeight(54);
    setShrink(0);
    setFocusable(true);
    setHideHighlight(true);
    setHideClickAnimation(true);
    setAlignItems(brls::AlignItems::CENTER);
    setJustifyContent(brls::JustifyContent::CENTER);
    registerClickAction([action = std::move(action)](brls::View*) { action(); return true; });
    updateActionHint(brls::BUTTON_A, title_);
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}
void ConsoleButton::SetTitle(const std::string& title)
{
    title_ = title;
    updateActionHint(brls::BUTTON_A, title);
}
void ConsoleButton::onFocusGained()
{
    brls::Box::onFocusGained();
    focus_.reset(); focus_.addStep(1, 160, brls::EasingFunction::cubicOut); focus_.start();
}
void ConsoleButton::onFocusLost()
{
    brls::Box::onFocusLost();
    focus_.reset(); focus_.addStep(0, 160, brls::EasingFunction::cubicOut); focus_.start();
}
void ConsoleButton::draw(NVGcontext* vg, float x, float y, float w, float h,
                         brls::Style style, brls::FrameContext* ctx)
{
    nvgSave(vg); nvgGlobalAlpha(vg, a(Text()).a);
    const float focus = focus_.getValue(), growth = primary_ ? 0.025f * focus : 0;
    x -= w * growth / 2; y -= h * growth / 2; w *= 1 + growth; h *= 1 + growth;
    const float radius = h / 2;
    DrawHalo(vg, x, y, w, h, radius, std::max(focus, active_ ? 0.55f : 0));
    nvgBeginPath(vg); nvgRoundedRect(vg, x, y, w, h, radius);
    nvgFillColor(vg, primary_ ? Text() : Opacity(Raised(), 0.86f)); nvgFill(vg);
    nvgStrokeWidth(vg, focus > 0.05f ? 2.5f : 1);
    nvgStrokeColor(vg, focus > 0.05f || active_ ? Opacity(Accent(), std::max(focus, 0.65f)) : Opacity(Text(), 0.18f)); nvgStroke(vg);
    const auto ink = primary_ ? Ground() : Text();
    if (primary_)
    {
        const float text_width = TextWidth(title_, 20, FontRole::Heading);
        const float start = x + (w - text_width - 32) / 2;
        DrawIcon(vg, icon_, start, y + (h - 20) / 2, 20, ink);
        nvgFontFaceId(vg, Font(FontRole::Heading)); nvgFontSize(vg, 20);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE); nvgFillColor(vg, ink);
        nvgText(vg, start + 32, y + h / 2, title_.c_str(), nullptr);
    }
    else DrawIcon(vg, icon_, x + (w - 24) / 2, y + (h - 24) / 2, 24, ink);
    if (verified_)
    {
        const float bx = x + w - 10, by = y + h - 10;
        nvgBeginPath(vg); nvgCircle(vg, bx, by, 9); nvgFillColor(vg, Raised()); nvgFill(vg);
        nvgBeginPath(vg); nvgMoveTo(vg, bx - 4, by); nvgLineTo(vg, bx - 1, by + 3); nvgLineTo(vg, bx + 5, by - 4);
        nvgStrokeColor(vg, Accent()); nvgStrokeWidth(vg, 2.5f); nvgStroke(vg);
    }
    nvgRestore(vg);
    brls::Box::draw(vg, x, y, w, h, style, ctx);
}

void DynamicBackdrop::Clear()
{
    blend_.stop(); blend_ = 0;
    waiting_ = false; transitioning_ = false;
    requested_.clear(); displayed_.clear();
    RetireArtwork(std::move(current_));
    RetireArtwork(std::move(next_));
}
void DynamicBackdrop::Select(const std::string& url)
{
    if (url == requested_) return;
    if (transitioning_ && blend_.getValue() > 0.5f)
    { RetireArtwork(std::move(current_)); current_ = std::move(next_); displayed_ = requested_; }
    blend_.stop(); blend_ = 0; transitioning_ = false;
    requested_ = url;
    if (current_ && displayed_ == url)
    { RetireArtwork(std::move(next_)); waiting_ = false; return; }
    RetireArtwork(std::move(next_));
    next_ = std::make_unique<CachedImage>();
    next_->SetArtwork(url);
    waiting_ = true;
}
void DynamicBackdrop::draw(NVGcontext* vg, float x, float y, float w, float h,
                           brls::Style, brls::FrameContext*)
{
    nvgSave(vg);
    nvgBeginPath(vg); nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + w, y + h,
        nvgLerpRGBA(Ground(), SecondaryAccent(), 0.14f), Ground()));
    nvgFill(vg);
    if (next_) next_->Poll();
    if (waiting_ && (requested_.empty() || next_->IsReady()))
    {
        blend_ = 0; blend_.addStep(1, 300, brls::EasingFunction::cubicOut); blend_.start();
        waiting_ = false; transitioning_ = true;
    }
    const float mix = transitioning_ ? blend_.getValue() : 0;
    if (current_) DrawArtwork(vg, *current_, x, y, w, h, true, requested_.empty() ? 1 - mix : 1);
    if (next_) DrawArtwork(vg, *next_, x, y, w, h, true, mix);
    // Black shading preserves the original colors; only the fallback is violet.
    nvgBeginPath(vg); nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 80)); nvgFill(vg);
    nvgBeginPath(vg); nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y + h * 0.33f, x, y + h,
        nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 245))); nvgFill(vg);
    nvgBeginPath(vg); nvgRect(vg, x, y, w, h * 0.24f);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x, y + h * 0.24f,
        nvgRGBA(0, 0, 0, 170), nvgRGBA(0, 0, 0, 0))); nvgFill(vg);
    if (transitioning_ && mix >= 0.999f)
    {
        RetireArtwork(std::move(current_));
        current_ = std::move(next_); displayed_ = requested_; transitioning_ = false;
    }
    nvgRestore(vg);
}
}
