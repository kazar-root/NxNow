#pragma once

#include "stream_settings.hpp"

#include <borealis.hpp>

#include <functional>
#include <string>

namespace nxnow::ui
{

enum class FontRole { Body, Medium, Heading, Display, Mono };
enum class ActionTone { Neutral, Primary, Destructive, Flat, Outlined };

inline NVGcolor Ground() { return nvgRGB(9, 7, 17); }
inline NVGcolor Raised() { return nvgRGB(23, 17, 36); }
inline NVGcolor Text() { return nvgRGB(255, 255, 255); }
inline NVGcolor Muted() { return nvgRGB(185, 179, 201); }
inline NVGcolor Rule() { return nvgRGB(51, 41, 70); }
inline NVGcolor Accent() { return nvgRGB(168, 85, 247); }
inline NVGcolor SecondaryAccent() { return nvgRGB(124, 58, 237); }
inline NVGcolor Danger() { return nvgRGB(255, 106, 85); }

void InitializeThemeAndFonts();
int Font(FontRole role);

class StyledLabel final : public brls::Label
{
  public:
    explicit StyledLabel(FontRole role = FontRole::Body);
};

brls::Label* MakeLabel(const std::string& text, float size = 18,
                      NVGcolor color = Text(), FontRole role = FontRole::Body);
float TextWidth(const std::string& text, float size, FontRole role);

class BlendedLogo final : public brls::View
{
  public:
    BlendedLogo();
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
  private:
    brls::Image image_;
};

class FadingScrollFrame final : public brls::ScrollingFrame
{
  public:
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
};

class ActionRow : public brls::Box
{
  public:
    ActionRow(std::string title, std::string value,
              std::function<bool(brls::View*)> activate,
              ActionTone tone = ActionTone::Neutral);
    void SetTitle(const std::string& title);
    void SetValue(const std::string& value);
    void SetFontSize(float size);
    void SetCentered(bool centered);
    void SetTone(ActionTone tone);
    void SetFocusHandler(std::function<void()> handler);
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    void UpdateChrome(bool focused);
    brls::Label* title_;
    brls::Label* value_;
    ActionTone tone_;
    std::function<void()> focus_handler_;
};

std::string StreamSummary(const StreamSettings& settings);
std::string ConfiguredLocation(const std::string& region);

class NextStreamSummaryView final : public brls::Box
{
  public:
    explicit NextStreamSummaryView(const StreamSettings& settings,
                                   std::function<void()> activate = {});
    void Update(const StreamSettings& settings);
    void onFocusGained() override;
    void onFocusLost() override;

  private:
    brls::Label* heading_;
    brls::Label* summary_;
    brls::Label* detail_;
    bool interactive_ = false;
};

}
