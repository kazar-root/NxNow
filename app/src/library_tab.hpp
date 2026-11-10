#pragma once
#include "gfn_client.hpp"
#include "game_carousel.hpp"
#include "library_favorites.hpp"
#include <atomic>
#include <chrono>
#include <memory>

namespace nxnow
{
namespace ui { class LoadingView; }
class LibraryTab : public brls::Box
{
  public:
    LibraryTab();
    ~LibraryTab() override;
    void willAppear(bool resetState) override;
    brls::View* getDefaultFocus() override;
    void SetBackdropHandler(std::function<void(const std::string&)> handler);
    void SetBackdropResetHandler(std::function<void()> handler) { reset_backdrop_ = std::move(handler); }
    void SetHeaderNavigation(brls::View* header);
    void SetFavoritesOnly(bool enabled);
    bool FavoritesOnly() const { return favorites_only_; }
    void BeginSearch();
    void RefreshAccount() { willAppear(false); }
    void SetAccountHandler(std::function<void()> handler) { account_handler_ = std::move(handler); }
    void ShowBrowseOptions();
    void SuspendArtwork();
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
  private:
    void EnsureSessionLoaded();
    void UpdateSessionUi();
    void ReloadLibrary(bool background = false);
    void RebuildList();
    void ChangePage(int delta);
    void ShowInitialLoading(bool enabled);
    void SelectGame(const std::string& identity);
    void UpdatePreview();
    void ToggleFavorite();
    bool OpenGameDialog(const std::string& identity, bool play = false);
    GfnClient client_;
    std::vector<GameInfo> games_;
    library::Favorites favorites_;
    GameCarouselView* carousel_ = nullptr;
    ui::ConsoleButton *search_button_ = nullptr, *play_button_ = nullptr,
                      *favorite_button_ = nullptr;
    brls::Label *heading_ = nullptr, *status_label_ = nullptr, *title_ = nullptr,
                *metadata_ = nullptr, *last_played_ = nullptr, *empty_ = nullptr;
    brls::Box* information_ = nullptr;
    brls::Box* connect_help_ = nullptr;
    ui::ConsoleButton* connect_profile_ = nullptr;
    brls::Label* connect_prompt_ = nullptr;
    std::function<void()> account_handler_;
    brls::Box *heading_container_ = nullptr, *pager_ = nullptr;
    ui::LoadingView* initial_loading_view_ = nullptr;
    ui::ActionRow *previous_page_ = nullptr, *next_page_ = nullptr;
    brls::Label* page_label_ = nullptr;
    std::function<void(const std::string&)> backdrop_;
    std::function<void()> reset_backdrop_;
    brls::View* header_navigation_ = nullptr;
    bool loading_ = false, rebuilding_ = false, favorites_only_ = false;
    bool initial_loading_ = false;
    bool artwork_suspended_ = false;
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
    std::string search_query_, selected_identity_, library_language_;
    size_t store_filter_index_ = 0, sort_mode_index_ = 0;
    size_t page_index_ = 0, page_count_ = 0, filtered_count_ = 0;
    std::chrono::steady_clock::time_point artwork_wait_started_ {};
    std::uint64_t library_session_generation_ = 0;
    std::chrono::steady_clock::time_point last_library_sync_ {};
};
}
