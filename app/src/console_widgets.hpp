#pragma once

#include "cover_image_cache.hpp"
#include "ui_theme.hpp"
#include <memory>

namespace nxnow::ui
{
enum class ConsoleIcon { Menu, Favorite, Library, Settings, Account, Info, Search, Play, Catalog };
NVGcolor Opacity(NVGcolor color, float alpha);
void DrawIcon(NVGcontext* vg, ConsoleIcon icon, float x, float y, float size, NVGcolor color);
void DrawHalo(NVGcontext* vg, float x, float y, float width, float height, float radius, float amount);
void DrawArtwork(NVGcontext* vg, CachedImage& image, float x, float y, float width,
                 float height, bool fill, float opacity = 1, float radius = 0);

class ConsoleButton final : public brls::Box
{
  public:
    ConsoleButton(ConsoleIcon icon, std::string title, std::function<void()> action,
                  bool primary = false);
    void SetTitle(const std::string& title);
    void SetActive(bool active) { active_ = active; }
    void SetVerified(bool verified) { verified_ = verified; }
    void onFocusGained() override;
    void onFocusLost() override;
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
  private:
    ConsoleIcon icon_;
    std::string title_;
    bool primary_, active_ = false;
    bool verified_ = false;
    brls::Animatable focus_ = 0;
};

class DynamicBackdrop final : public brls::View
{
  public:
    void Select(const std::string& url);
    void Clear();
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
  private:
    std::unique_ptr<CachedImage> current_, next_;
    std::string requested_, displayed_;
    brls::Animatable blend_ = 0;
    bool waiting_ = false, transitioning_ = false;
};
}
