#pragma once

#include "localization.hpp"
#include "ui_theme.hpp"
#include <chrono>
#include <cmath>

namespace nxnow::ui
{
class LoadingDotsView final : public brls::View
{
  public:
    LoadingDotsView() { setWidth(84); setHeight(28); setShrink(0); }
    void draw(NVGcontext* vg, float x, float y, float w, float h,
              brls::Style, brls::FrameContext*) override
    {
        const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - started_).count();
        const float phase = std::fmod(elapsed, 1.2f) / 0.4f;
        nvgSave(vg);
        for (int i = 0; i < 3; ++i)
        {
            auto color = Accent();
            color.a *= i == static_cast<int>(phase) ? 0.55f + 0.45f * std::sin((phase - i) * 3.14159265f) : 0.2f;
            nvgBeginPath(vg);
            nvgCircle(vg, x + w / 2 + (i - 1) * 22, y + h / 2, 5);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
        nvgRestore(vg);
    }
  private:
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
};

class LoadingView final : public brls::Box
{
  public:
    LoadingView() : brls::Box(brls::Axis::COLUMN)
    {
        setBackgroundColor(Ground());
        setWidthPercentage(100);
        setGrow(1); setMinHeight(0);
        setAlignItems(brls::AlignItems::CENTER);
        setJustifyContent(brls::JustifyContent::CENTER);
        setFocusable(true); setHideHighlight(true);
        auto* logo = new brls::Image();
        logo->setWidth(200); logo->setHeight(200); logo->setShrink(0);
        logo->setScalingType(brls::ImageScalingType::FIT);
        logo->setImageFromRes("img/nxnow-logo-mark.png");
        logo->setMarginBottom(14); addView(logo);
        auto* caption = MakeLabel(Tr("Loading..."), 26, Text(), FontRole::Heading);
        caption_ = caption;
        caption->setId("loading-caption"); caption->setShrink(0);
        caption->setMarginBottom(12); addView(caption);
        addView(new LoadingDotsView());
    }
    void RefreshLanguage() { caption_->setText(Tr("Loading...")); }
  private:
    brls::Label* caption_ = nullptr;
};
}
