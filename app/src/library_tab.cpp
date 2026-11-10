#include "library_tab.hpp"
#include "manual_library_policy.hpp"
#include "app_state.hpp"
#include "game_detail_view.hpp"
#include "game_detail_policy.hpp"
#include "library_sort.hpp"
#include "library_timetable_policy.hpp"
#include "localization.hpp"
#include "ui_action_guard.hpp"
#include "ui_helpers.hpp"
#include "ui_loading_view.hpp"
#include "carousel_policy.hpp"
#include <algorithm>
#include <array>
#include <cctype>

namespace nxnow
{
namespace
{
constexpr std::array<const char*, 6> kStoreFilters = {"All", "Steam", "Epic", "Ubisoft", "Xbox", "Battle.net"};
constexpr std::array<const char*, 4> kSortModes = {"Last Played", "Last Added", "A-Z", "Store"};
std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
std::string GameIdentity(const GameInfo& game) { return game.uuid.empty() ? game.id : game.uuid; }
std::string PrimaryStore(const GameInfo& game)
{
    for (const auto& store : game.available_stores)
        if (!game_detail::IsUnknownMetadata(store)) return game_detail::DisplayStore(store);
    return "GeForce NOW";
}
std::string ActiveUser()
{
    const auto& session = AppState::Instance().session();
    return session ? session->user.user_id : std::string {};
}
}
LibraryTab::LibraryTab() : brls::Box(brls::Axis::COLUMN)
{
    setId("library"); setPadding(0, 0, 8, 0);
    library_session_generation_ = AppState::Instance().session_generation();
    auto* heading = new brls::Box(brls::Axis::ROW);
    heading_container_ = heading;
    heading->setHeight(44); heading->setShrink(0); heading->setPadding(0, 64, 0, 64);
    heading->setAlignItems(brls::AlignItems::CENTER);
    heading_ = ui::MakeLabel(Tr("My Library"), 16, ui::Text(), ui::FontRole::Heading);
    heading_->setGrow(1); heading->addView(heading_);
    status_label_ = ui::MakeLabel({}, 14, ui::Muted());
    status_label_->setId("library-status"); status_label_->setSingleLine(true);
    status_label_->setMaxWidth(560); status_label_->setShrink(1); status_label_->setMarginRight(18);
    heading->addView(status_label_);
    search_button_ = new ui::ConsoleButton(ui::ConsoleIcon::Search, Tr("Search"), [this] { BeginSearch(); });
    search_button_->setId("library-search"); search_button_->setWidth(40); search_button_->setHeight(40);
    heading->addView(search_button_); addView(heading);
    carousel_ = new GameCarouselView([this](const std::string& id) { SelectGame(id); },
        [this] { OpenGameDialog(selected_identity_); }, [this] { ChangePage(1); }, [this] { ChangePage(-1); });
    addView(carousel_);
    empty_ = ui::MakeLabel({}, 22, ui::Text(), ui::FontRole::Heading);
    empty_->setId("library-empty"); empty_->setMargins(64, 64, 64, 64);
    empty_->setGrow(1); empty_->setVisibility(brls::Visibility::GONE); addView(empty_);
    connect_help_ = new brls::Box(brls::Axis::COLUMN);
    connect_help_->setId("library/connect-help");
    connect_help_->setGrow(1);
    connect_help_->setPadding(36, 64, 36, 64);
    connect_help_->setAlignItems(brls::AlignItems::CENTER);
    connect_help_->setJustifyContent(brls::JustifyContent::CENTER);
    connect_profile_ = new ui::ConsoleButton(ui::ConsoleIcon::Account, Tr("Sign in with QR code"), [this] {
        if (account_handler_) account_handler_();
    });
    connect_profile_->setId("library/connect-profile");
    connect_profile_->setWidth(72); connect_profile_->setHeight(72);
    connect_profile_->setMarginBottom(22);
    connect_help_->addView(connect_profile_);
    connect_prompt_ = ui::MakeLabel(Tr("Click the profile icon to sign in and find your games."), 24, ui::Text(), ui::FontRole::Heading);
    connect_prompt_->setId("library/connect-prompt");
    connect_prompt_->setWidthPercentage(80);
    connect_prompt_->setSingleLine(false);
    connect_prompt_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    connect_help_->addView(connect_prompt_);
    connect_help_->setVisibility(AppState::Instance().HasSession() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    addView(connect_help_);
    information_ = new brls::Box(brls::Axis::COLUMN);
    information_->setId("library-information"); information_->setGrow(1); information_->setMinHeight(0);
    information_->setPadding(0, 64, 0, 64);
    title_ = ui::MakeLabel({}, 36, ui::Text(), ui::FontRole::Display);
    title_->setId("library-title"); title_->setHeight(47); title_->setShrink(0); title_->setSingleLine(true);
    information_->addView(title_);
    metadata_ = ui::MakeLabel({}, 15, ui::Muted()); metadata_->setId("library-metadata");
    metadata_->setHeight(24); metadata_->setSingleLine(true); information_->addView(metadata_);
    last_played_ = ui::MakeLabel({}, 13, ui::Muted()); last_played_->setHeight(22);
    last_played_->setSingleLine(true); information_->addView(last_played_);
    auto* actions = new brls::Box(brls::Axis::ROW); actions->setId("library-actions");
    actions->setHeight(62); actions->setShrink(0); actions->setAlignItems(brls::AlignItems::CENTER);
    actions->setJustifyContent(brls::JustifyContent::CENTER);
    play_button_ = new ui::ConsoleButton(ui::ConsoleIcon::Play, Tr("PLAY"),
        [this] { OpenGameDialog(selected_identity_, true); }, true);
    play_button_->setId("library-play"); play_button_->setMarginRight(14); actions->addView(play_button_);
    favorite_button_ = new ui::ConsoleButton(ui::ConsoleIcon::Favorite, Tr("Favorites"), [this] { ToggleFavorite(); });
    favorite_button_->setId("library-favorite"); favorite_button_->setMarginRight(12); actions->addView(favorite_button_);
    information_->addView(actions); addView(information_);
    pager_ = new brls::Box(brls::Axis::ROW);
    pager_->setId("library/pager"); pager_->setHeight(46); pager_->setShrink(0);
    pager_->setAlignItems(brls::AlignItems::CENTER);
    previous_page_ = new ui::ActionRow(Tr("Previous"), {}, [this](brls::View*) { ChangePage(-1); return true; });
    previous_page_->setId("library/previous"); previous_page_->setWidth(150); previous_page_->setHeight(46);
    pager_->addView(previous_page_);
    page_label_ = ui::MakeLabel({}, 14, ui::Text());
    page_label_->setId("library/page"); page_label_->setWidth(90);
    page_label_->setHorizontalAlign(brls::HorizontalAlign::CENTER); pager_->addView(page_label_);
    next_page_ = new ui::ActionRow(Tr("Next"), {}, [this](brls::View*) { ChangePage(1); return true; });
    next_page_->setId("library/next"); next_page_->setWidth(150); next_page_->setHeight(46);
    pager_->addView(next_page_); actions->addView(pager_);
    initial_loading_view_ = new ui::LoadingView();
    initial_loading_view_->setId("library/loading");
    initial_loading_view_->setVisibility(brls::Visibility::GONE); addView(initial_loading_view_);
    carousel_->setCustomNavigationRoute(brls::FocusDirection::DOWN, play_button_);
    search_button_->setCustomNavigationRoute(brls::FocusDirection::DOWN, carousel_);
    for (auto* button : {play_button_, favorite_button_})
        button->setCustomNavigationRoute(brls::FocusDirection::UP, carousel_);
    registerAction(Tr("Refresh"), brls::BUTTON_Y, [this](brls::View*) { ReloadLibrary(); return true; });
    registerAction({}, brls::BUTTON_LB, [this](brls::View*) { ChangePage(-1); return true; }, true);
    registerAction({}, brls::BUTTON_RB, [this](brls::View*) { ChangePage(1); return true; }, true);
}
LibraryTab::~LibraryTab() { alive_->store(false); }
brls::View* LibraryTab::getDefaultFocus()
{ return !AppState::Instance().HasSession() ? static_cast<brls::View*>(connect_profile_) : initial_loading_ ? initial_loading_view_ : carousel_->ItemCount() ? static_cast<brls::View*>(carousel_) : search_button_; }
void LibraryTab::SetBackdropHandler(std::function<void(const std::string&)> handler) { backdrop_ = std::move(handler); UpdatePreview(); }
void LibraryTab::SetHeaderNavigation(brls::View* header)
{
    header_navigation_ = header;
    // Borealis rejects routes on a disabled/hidden source. At cold startup
    // the catalog is empty until the asynchronous library request completes.
    if (carousel_->isFocusable())
        carousel_->setCustomNavigationRoute(brls::FocusDirection::UP, header);
    if (search_button_->isFocusable())
    {
        search_button_->setCustomNavigationRoute(brls::FocusDirection::UP, header);
        search_button_->setCustomNavigationRoute(brls::FocusDirection::DOWN,
            carousel_->isFocusable() ? static_cast<brls::View*>(carousel_) : search_button_);
    }
    if (connect_profile_->isFocusable())
        connect_profile_->setCustomNavigationRoute(brls::FocusDirection::UP, header);
    if (initial_loading_view_->isFocusable())
        initial_loading_view_->setCustomNavigationRoute(brls::FocusDirection::UP, header);
}
void LibraryTab::SetFavoritesOnly(bool enabled)
{
    favorites_only_ = enabled; page_index_ = 0; selected_identity_.clear();
    RebuildList(); brls::Application::giveFocus(getDefaultFocus());
}
void LibraryTab::BeginSearch()
{
    const auto alive = alive_;
    const auto generation = AppState::Instance().session_generation();
    brls::Application::getImeManager()->openForText([this, alive, generation](std::string text) {
        if (!alive->load() || AppState::Instance().session_generation() != generation) return;
        search_query_ = std::move(text); page_index_ = 0; selected_identity_.clear(); RebuildList();
        brls::Application::giveFocus(getDefaultFocus());
    }, Tr("Search Library"), Tr("Enter a title to filter your games"), 64, search_query_);
}
void LibraryTab::willAppear(bool resetState)
{
    artwork_suspended_ = false;
    brls::Box::willAppear(resetState); EnsureSessionLoaded();
    auto& state = AppState::Instance();
    if (library_language_ != GetInterfaceLanguage())
    {
        library_language_ = GetInterfaceLanguage();
        last_library_sync_ = {};
    }
    if (library_session_generation_ != state.session_generation())
    {
        loading_ = false;
        initial_loading_ = false;
        initial_loading_view_->setVisibility(brls::Visibility::GONE);
        library_session_generation_ = state.session_generation(); selected_identity_.clear();
        search_query_.clear(); favorites_only_ = false; page_index_ = 0; last_library_sync_ = {};
    }
    games_ = state.HasSession() ? state.library_games() : std::vector<GameInfo> {};
    if (state.HasSession())
    {
        library::MergeManualGames(games_, client_.LoadManualLibraryGames(*state.session()));
        state.SetLibraryGames(games_);
    }
    favorites_ = library::LoadFavorites(ActiveUser()); RebuildList();
    const auto now = std::chrono::steady_clock::now();
    const bool due = last_library_sync_.time_since_epoch().count() == 0 || now - last_library_sync_ >= std::chrono::minutes(2);
    if (state.HasSession() && !state.session()->reauthentication_required && !loading_ && (games_.empty() || due))
        ReloadLibrary(!games_.empty());
}
void LibraryTab::EnsureSessionLoaded()
{
    auto& state = AppState::Instance();
    if (state.IsSessionLoaded())
        return;

    AuthSession session;
    if (client_.LoadSavedSession(session))
        state.SetSession(std::move(session));
    else
        state.MarkSessionLoaded();
}

void LibraryTab::SuspendArtwork()
{
    artwork_suspended_ = true;
    carousel_->SetGames({}, {});
}

void LibraryTab::UpdateSessionUi()
{
    const auto& state = AppState::Instance();
    if (!state.HasSession())
    {
        status_label_->setText(Tr("Sign in to find your games"));
        connect_prompt_->setText(Tr("Click the profile icon to sign in and find your games."));
        connect_profile_->SetTitle(Tr("Sign in with QR code"));
        return;
    }

    const AuthSession& session = *state.session();

    if (session.reauthentication_required)
    {
        status_label_->setText(
            Tr("Click the profile icon to reconnect your account."));
    }
}

void LibraryTab::ReloadLibrary(bool background)
{
    auto& state = AppState::Instance();
    if (loading_ || !state.HasSession() || state.session()->reauthentication_required)
    {
        UpdateSessionUi();
        RebuildList();
        return;
    }

    loading_ = true;
    if (games_.empty())
        ShowInitialLoading(true);
    // Throttle automatic retries even when NVIDIA temporarily rejects a
    // refresh, otherwise every tab appearance immediately repeats it.
    last_library_sync_ = std::chrono::steady_clock::now();
    status_label_->setText(Tr("Syncing your GeForce NOW library..."));

    AuthSession session = *state.session();
    const auto generation = state.session_generation();
    GfnClient client = client_;
    const auto alive = alive_;
    const std::string language = GetInterfaceLanguage();
    brls::async([this, alive, client, session = std::move(session), background, generation, language]() mutable {
        try
        {
            std::vector<GameInfo> games = client.FetchLibraryGames(session, language);
            brls::sync([this, alive, games = std::move(games), session = std::move(session), background, generation, language]() mutable {
                if (!alive->load())
                    return;
                auto& current = AppState::Instance();
                if (!current.IsCurrentSession(generation) ||
                    current.session()->user.user_id != session.user.user_id)
                    return;
                if (language != GetInterfaceLanguage())
                {
                    loading_ = false;
                    last_library_sync_ = {};
                    ReloadLibrary(background);
                    return;
                }
                games_ = std::move(games);
                artwork_wait_started_ = std::chrono::steady_clock::now();
                last_library_sync_ = std::chrono::steady_clock::now();
                current.SetSession(std::move(session));
                current.SetLibraryGames(games_);
                loading_ = false;
                library_session_generation_ = generation;
                RebuildList();
                if (!background)
                    brls::Application::notify(Tr("Library refreshed"));
            });
        }
        catch (const std::exception& ex)
        {
            const std::string error = ex.what();
            brls::sync([this, alive, error, background, generation]() {
                if (!alive->load())
                    return;
                if (!AppState::Instance().IsCurrentSession(generation))
                    return;
                loading_ = false;
                ShowInitialLoading(false);
                if (background && !games_.empty())
                {
                    status_label_->setText(
                        Tr("Showing cached library. NVIDIA background refresh is temporarily unavailable."));
                    return;
                }
                ShowError("Library Sync Failed", error);
                UpdateSessionUi();
            });
        }
    }, false);
}


void LibraryTab::RebuildList()
{
    if (rebuilding_) return;
    rebuilding_ = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset {rebuilding_};
    const auto& state = AppState::Instance();
    heading_->setText(Tr(favorites_only_ ? "Favorites" : "My Library"));
    search_button_->SetTitle(Tr("Search")); play_button_->SetTitle(Tr("PLAY"));
    updateActionHint(brls::BUTTON_Y, Tr("Refresh"));
    initial_loading_view_->RefreshLanguage();
    previous_page_->SetTitle(Tr("Previous")); next_page_->SetTitle(Tr("Next"));
    std::vector<size_t> indices;
    const auto query = ToLower(search_query_);
    for (size_t i = 0; i < games_.size(); ++i)
    {
        const auto& game = games_[i];
        if (!query.empty() && ToLower(game.title).find(query) == std::string::npos) continue;
        if (favorites_only_ && !favorites_.contains(GameIdentity(game))) continue;
        if (store_filter_index_ > 0)
        {
            const auto store = ToLower(kStoreFilters[store_filter_index_]);
            bool matches = ToLower(PrimaryStore(game)).find(store) != std::string::npos;
            for (const auto& candidate : game.available_stores)
                matches = matches || ToLower(game_detail::DisplayStore(candidate)).find(store) != std::string::npos;
            if (!matches) continue;
        }
        indices.push_back(i);
    }
    SortLibraryIndices(indices, games_, static_cast<LibrarySortMode>(sort_mode_index_));
    filtered_count_ = indices.size();
    page_count_ = library::CarouselPageCount(filtered_count_);
    favorite_button_->setMarginRight(page_count_ > 1 ? 24 : 0);
    page_index_ = page_count_ ? std::min(page_index_, page_count_ - 1) : 0;
    const auto [first, last] = library::CarouselPageRange(page_index_, filtered_count_);
    const bool hidden_pager_focus = page_count_ <= 1 && IsViewInside(pager_, brls::Application::getCurrentFocus());
    pager_->setVisibility(page_count_ > 1 ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    previous_page_->setAlpha(page_index_ > 0 ? 1 : 0.4f);
    next_page_->setAlpha(page_index_ + 1 < page_count_ ? 1 : 0.4f);
    page_label_->setText(std::to_string(page_index_ + 1) + " / " + std::to_string(page_count_));
    std::vector<CarouselItem> items;
    for (size_t slot = first; slot < last; ++slot)
    {
        const auto& game = games_[indices[slot]];
        items.push_back({GameIdentity(game), game.title, game.portrait_url.empty() ? game.image_url : game.portrait_url});
    }
    const bool empty = items.empty();
    // A stable viewport owns focus while its bounded texture window changes.
    if (empty && IsViewInside(information_, brls::Application::getCurrentFocus())) brls::Application::giveFocus(search_button_);
    if (empty && brls::Application::getCurrentFocus() == carousel_) brls::Application::giveFocus(search_button_);
    heading_container_->setVisibility(initial_loading_ ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    carousel_->setVisibility(empty || initial_loading_ ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    information_->setVisibility(empty || initial_loading_ ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
    connect_help_->setVisibility(!state.HasSession() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    empty_->setVisibility(state.HasSession() && empty && !initial_loading_ ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (empty)
    {
        selected_identity_.clear();
        empty_->setText(Tr(!state.HasSession() ? "Connect your account to discover your games." :
            favorites_only_ ? "No favorites yet. Use the star on a game to add it." :
            "No games found matching your search."));
    }
    if (artwork_suspended_) carousel_->SetGames({}, {});
    else carousel_->SetGames(std::move(items), selected_identity_, page_index_ + 1 < page_count_, page_index_ > 0);
    if (hidden_pager_focus) brls::Application::giveFocus(getDefaultFocus());
    if (header_navigation_)
        SetHeaderNavigation(header_navigation_);
    UpdatePreview(); UpdateSessionUi();
    rebuilding_ = false;
}
void LibraryTab::SelectGame(const std::string& identity) { selected_identity_ = identity; UpdatePreview(); }
void LibraryTab::UpdatePreview()
{
    if (artwork_suspended_) return;
    const auto found = std::find_if(games_.begin(), games_.end(), [this](const auto& game) { return GameIdentity(game) == selected_identity_; });
    if (found == games_.end())
    {
        title_->setText({}); metadata_->setText({}); last_played_->setText({});
        if (backdrop_) backdrop_({});
        return;
    }
    title_->setText(found->title);
    std::string metadata = PrimaryStore(*found);
    if (!game_detail::IsUnknownMetadata(found->publisher)) metadata += "  ·  " + found->publisher;
    metadata_->setText(metadata);
    last_played_->setText(found->last_played.empty() ? "" : Tr("Last played") + "  ·  " + game_detail::FormatLastPlayed(found->last_played));
    favorite_button_->SetActive(favorites_.contains(selected_identity_));
    favorite_button_->SetTitle(Tr(favorites_.contains(selected_identity_) ? "Remove favorite" : "Add favorite"));
    status_label_->setText(TrFormat("{0} games · Page {1}/{2}", {std::to_string(filtered_count_),
        std::to_string(page_index_ + 1), std::to_string(page_count_)}));
    if (backdrop_ && !initial_loading_)
        backdrop_(found->background_url.empty() ? found->image_url : found->background_url);
}
void LibraryTab::ToggleFavorite()
{
    if (!AppState::Instance().IsCurrentSession(library_session_generation_) || selected_identity_.empty()) return;
    const bool favorite = !favorites_.contains(selected_identity_);
    if (!library::SetFavorite(ActiveUser(), selected_identity_, favorite))
    { ShowError("Favorites", "Could not save favorites."); return; }
    if (favorite) favorites_.insert(selected_identity_); else favorites_.erase(selected_identity_);
    if (favorites_only_) RebuildList(); else UpdatePreview();
}
bool LibraryTab::OpenGameDialog(const std::string& identity, bool play)
{
    if (!AppState::Instance().IsCurrentSession(library_session_generation_)) return false;
    const auto found = std::find_if(games_.begin(), games_.end(), [&](const auto& game) { return GameIdentity(game) == identity; });
    if (found == games_.end()) return false;
    auto* detail = new GameDetailView(client_, MakeLibraryGameDetail(*found), !play);
    brls::Application::pushActivity(new brls::Activity(detail), play ? brls::TransitionAnimation::NONE : brls::TransitionAnimation::FADE);
    if (play) detail->RequestPlay();
    return true;
}
void LibraryTab::ShowBrowseOptions()
{
    std::vector<std::string> options;
    for (const char* sort : kSortModes) options.push_back(Tr(sort));
    for (const char* store : kStoreFilters) options.push_back(Tr("Store") + " · " + Tr(store));
    const auto alive = alive_;
    auto* dropdown = new brls::Dropdown(Tr("Library options"), options, [](int) {}, 0,
        [this, alive](int choice) {
            if (!alive->load() || choice < 0) return;
            page_index_ = 0; selected_identity_.clear();
            if (choice < static_cast<int>(kSortModes.size())) sort_mode_index_ = static_cast<size_t>(choice);
            else if (static_cast<size_t>(choice) - kSortModes.size() < kStoreFilters.size())
                store_filter_index_ = static_cast<size_t>(choice) - kSortModes.size();
            RebuildList(); brls::Application::giveFocus(getDefaultFocus());
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}
void LibraryTab::ChangePage(int delta)
{
    if (initial_loading_ || page_count_ < 2) return;
    const auto next = library::CarouselTarget(page_index_, delta, page_count_);
    if (next == page_index_) return;
    page_index_ = next;
    selected_identity_.clear();
    // Drop the previous page before asking for even one new cover.
    carousel_->SetGames({}, {});
    if (reset_backdrop_) reset_backdrop_();
    RebuildList();
    brls::Application::giveFocus(carousel_);
}
void LibraryTab::ShowInitialLoading(bool enabled)
{
    initial_loading_ = enabled;
    initial_loading_view_->setVisibility(enabled ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    const bool owns_focus = brls::Application::getCurrentFocus() == initial_loading_view_ ||
        IsViewInside(this, brls::Application::getCurrentFocus());
    RebuildList();
    if (owns_focus) brls::Application::giveFocus(getDefaultFocus());
}
void LibraryTab::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style, brls::FrameContext* ctx)
{
    if (initial_loading_ && !loading_)
    {
        const bool settled = carousel_->PollArtwork();
        const bool timed_out = std::chrono::steady_clock::now() - artwork_wait_started_ >= std::chrono::seconds(12);
        if (settled || timed_out) ShowInitialLoading(false);
    }
    brls::Box::draw(vg, x, y, w, h, style, ctx);
}
}
