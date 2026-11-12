#pragma once

#include "gfn_client.hpp"
#include "cover_image_cache.hpp"

#include <borealis.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "stream_settings.hpp"
#include "ui_theme.hpp"

namespace nxnow
{

class SettingsTab : public brls::Box
{
  public:
    SettingsTab();
    ~SettingsTab() override;

    void willAppear(bool resetState) override;
    bool OpenAccountMenu();
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    void EnsureSessionLoaded();
    void RefreshSummary();
    void RebuildSettings();
    void BuildAccountPage();
    void BuildStreamPage();
    void BuildPreferencesPage();
    void BuildAppPage();
    void BuildAdvancedPage();
    void UpdateOptionValues();
    void ShowOptionHelp(const std::string& description);
    void UpdateOptionHelp();
    void MarkDirty();
    bool SaveChanges(brls::View* view);
    bool CycleResolution(brls::View* view);
    bool ChooseQualityPreset(brls::View* view);
    bool CycleFrameRate(brls::View* view);
    bool CycleBitrate(brls::View* view);
    bool CycleVideoBackend(brls::View* view);
    bool CycleImageQuality(brls::View* view);
    bool ToggleStatsOverlay(brls::View* view);
    bool ToggleDiagnostics(brls::View* view);
    bool ChooseServerLocation(brls::View* view);
    bool RefreshServerLocations(brls::View* view);
    void BeginServerLocationLoad(bool open_when_ready, bool notify_result);
    void SyncServerLocationAccount();
    std::string ServerLocationValue() const;
    enum class CacheAction { Inspect, Clear };
    void BeginCoverCacheWork(CacheAction action);
    void UpdateCoverCacheValues();

    static brls::Label* MakeParagraph(
        const std::string& text, float bottom_margin = 16.0f);
    static std::string FormatBytes(std::uint64_t bytes);
    static std::string BitrateValue(int kbps);
    brls::Box* MakeSection(const std::string& title, const std::string& subtitle = {});
    brls::Box* MakeOptionRow(
        const std::string& title,
        const std::string& description,
        std::function<std::string()> value,
        std::function<bool(brls::View*)> action);
    brls::Box* MakeActionRow(
        const std::string& title,
        const std::string& description,
        const std::string& button_text,
        std::function<bool(brls::View*)> action,
        bool destructive = false);
    brls::Label* AddInfoLine(
        brls::Box* parent, const std::string& label, const std::string& value);

    bool ClearSavedLogin(brls::View* view);
    bool BeginLogin(brls::View* view);
    bool ClearCoverCache(brls::View* view);
    bool ChooseGameLanguage(brls::View* view);
    bool TogglePersistGameSettings(brls::View* view);
    bool ToggleControllerLayout(brls::View* view);
    bool ToggleAudio(brls::View* view);
    bool CycleAudioVolume(brls::View* view);
    bool CycleQueueNotifyThreshold(brls::View* view);
    bool ChooseInterfaceLanguage(brls::View* view);
    bool ChooseMouseSpeed(brls::View* view);
    bool ShowHomeScreenHelp(brls::View* view);
    bool CheckForUpdates(brls::View* view);
    bool update_check_running_ = false;

    GfnClient client_;
    brls::Label* page_title_ = nullptr;
    brls::Label* page_subtitle_ = nullptr;
    brls::Label* save_status_ = nullptr;
    brls::Label* help_description_ = nullptr;
    std::string help_description_text_;
    brls::ScrollingFrame* scrolling_frame_ = nullptr;
    brls::Box* content_container_ = nullptr;
    std::vector<std::pair<ui::ActionRow*, std::function<std::string()>>> option_values_;
    StreamSettings saved_settings_;
    StreamSettings draft_settings_;
    bool settings_loaded_ = false;
    bool dirty_ = false;
    bool rebuild_pending_ = false;
    bool advanced_visible_ = false;
    bool focus_advanced_after_rebuild_ = false;
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
    brls::Label* cover_cache_files_ = nullptr;
    brls::Label* cover_cache_bytes_ = nullptr;
    std::optional<CoverImageCacheStats> cover_cache_stats_;
    std::optional<CacheAction> cover_cache_action_;
    std::vector<StreamRegion> server_locations_;
    std::optional<std::uint64_t> server_locations_generation_;
    bool server_locations_loaded_ = false;
    bool server_locations_loading_ = false;
    bool open_server_location_when_ready_ = false;
    std::string server_locations_error_;
};

} // namespace nxnow
