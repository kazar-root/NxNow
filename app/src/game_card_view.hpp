#pragma once

#include <borealis.hpp>

#include <functional>
#include <string>

namespace nxnow
{

class CachedImage;

struct GameCardDisplay
{
    std::string title;
    std::string subtitle;
    std::string badge;
    std::string image_url;
    bool in_library = false;
    bool compact = false;
};

class GameCardView : public brls::Box
{
  public:
    using ClickHandler = std::function<void()>;

    GameCardView(GameCardDisplay display, ClickHandler click_handler);

    void onFocusGained() override;
    void onFocusLost() override;
    void SetFocusHandler(std::function<void()> handler);

  private:
    void UpdateChrome(bool focused);
    void LoadImage();

    GameCardDisplay display_;
    ClickHandler click_handler_;
    std::function<void()> focus_handler_;
    CachedImage* image_         = nullptr;
    brls::Label* title_label_   = nullptr;
    brls::Label* subtitle_label_ = nullptr;
};

} // namespace nxnow
