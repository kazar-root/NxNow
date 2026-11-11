#pragma once

#include "gfn_client.hpp"
#include "models.hpp"
#include "ui_theme.hpp"

#include <borealis.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nxnow
{

struct GameDetailData
{
    std::string title;
    std::string game_id;
    std::string subtitle;
    std::string image_url;
    std::string launch_app_id;
    std::string publisher;
    std::string description;
    std::string stores;
    std::string membership_tier_label;
    std::string last_played;
    bool owned = false;
    bool free_to_play = false;
    std::vector<GameVariant> variants;
    std::string description_language;
};

class GameDetailView : public brls::Box
{
  public:
    GameDetailView(const GfnClient& client, GameDetailData data, bool load_description = true);
    ~GameDetailView() override;
    brls::View* getDefaultFocus() override;
    void RequestPlay();

  private:
    void Play();
    void ChooseStreamPreset();
    void ShowLauncherSelector();
    void LaunchSelectedVariant();
    void DeferredLaunchSelectedVariant(std::uint64_t generation,
                                      const std::string& launch_app_id);
    void CreateSwitchShortcut();
    void AddToLibrary();
    void ConfirmRemoveFromLibrary();
    void RemoveFromLibrary();
    void OpenNteCredentialsMenu();
    void ConfigureNteCredentials();
    void UpdateNteButton();
    void LoadDescription();
    std::string ActiveUserId() const;

    GfnClient client_;
    GameDetailData data_;
    ui::ActionRow* play_button_ = nullptr;
    ui::ActionRow* nte_button_ = nullptr;
    ui::ActionRow* add_library_button_ = nullptr;
    ui::ActionRow* remove_library_button_ = nullptr;
    ui::NextStreamSummaryView* stream_summary_ = nullptr;
    brls::Label* description_label_ = nullptr;
    std::shared_ptr<std::atomic_bool> alive_ =
        std::make_shared<std::atomic_bool>(true);
    size_t selected_variant_index_ = 0;
    bool launcher_preference_loaded_ = false;
    std::uint64_t account_generation_ = 0;
};

GameDetailData MakeLibraryGameDetail(const GameInfo& game);
GameDetailData MakeCatalogGameDetail(const PublicGame& game);

} // namespace nxnow
