#pragma once
#include "console_widgets.hpp"
#include <functional>
#include <vector>

namespace nxnow
{
struct CarouselItem { std::string id, title, image_url; };

class GameCarouselView final : public brls::View
{
  public:
    GameCarouselView(std::function<void(const std::string&)> select, std::function<void()> activate,
                     std::function<void()> next_page = {}, std::function<void()> previous_page = {});
    void SetGames(std::vector<CarouselItem> games, const std::string& selected_id,
                  bool has_next_page = false, bool has_previous_page = false);
    void MoveSelection(int delta);
    std::string SelectedIdentity() const;
    std::size_t ItemCount() const { return items_.size(); }
    std::size_t ResidentCount() const { return slots_.size(); }
    bool PollArtwork();
    bool IsNextPageSelected() const { return has_next_page_ && selected_ == items_.size() + has_previous_page_; }
    bool IsPreviousPageSelected() const { return has_previous_page_ && selected_ == 0; }
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
  private:
    void Select(std::size_t index, bool animate);
    void ReconcileTextures(bool selected_only = false);
    void ActivateSelected();
    std::size_t SelectedImageIndex() const;
    struct Slot { std::string id, url; std::unique_ptr<CachedImage> image; };
    std::vector<CarouselItem> items_;
    std::vector<Slot> slots_;
    std::function<void(const std::string&)> select_;
    std::function<void()> activate_;
    std::function<void()> next_page_;
    std::function<void()> previous_page_;
    bool has_next_page_ = false, has_previous_page_ = false;
    std::size_t selected_ = 0;
    std::size_t previous_selected_ = 0;
    brls::Animatable position_ = 0;
    brls::Animatable selection_mix_ = 1;
};
}
