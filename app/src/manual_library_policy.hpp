#pragma once
#include "models.hpp"
#include <algorithm>

namespace nxnow::library
{
inline GameInfo ManualGameInfo(const PublicGame& item)
{
    GameInfo game;
    game.id = item.id; game.uuid = item.uuid; game.title = item.title;
    game.launch_app_id = item.launch_app_id.empty() ? item.id : item.launch_app_id;
    game.publisher = item.publisher; game.image_url = item.image_url;
    game.description = item.description; game.description_language = item.description_language;
    game.is_in_library = true; game.variants = item.variants;
    if (game.variants.empty())
    {
        GameVariant variant;
        variant.id = game.launch_app_id; variant.store = item.store; variant.library_status = "MANUAL";
        game.variants.push_back(std::move(variant));
    }
    for (const auto& variant : game.variants) game.available_stores.push_back(variant.store);
    return game;
}
inline bool SameGame(const PublicGame& item, const GameInfo& game)
{
    if (item.id == game.id || (!item.uuid.empty() && item.uuid == game.uuid)) return true;
    const auto launch_id = item.launch_app_id.empty() ? item.id : item.launch_app_id;
    return !launch_id.empty() && (launch_id == game.launch_app_id ||
        std::any_of(game.variants.begin(), game.variants.end(), [&](const auto& variant) {
            return variant.id == launch_id;
        }));
}
// Identifiers that designate a library game (used to remove / hide it).
inline std::vector<std::string> LibraryGameIds(const std::string& game_id, const std::string& launch_app_id)
{
    std::vector<std::string> ids;
    if (!game_id.empty()) ids.push_back(game_id);
    if (!launch_app_id.empty() && launch_app_id != game_id) ids.push_back(launch_app_id);
    return ids;
}
inline bool MatchesLibraryIds(const GameInfo& game, const std::vector<std::string>& ids)
{
    const auto has = [&](const std::string& value) {
        return !value.empty() && std::find(ids.begin(), ids.end(), value) != ids.end();
    };
    return has(game.id) || has(game.uuid) || has(game.launch_app_id);
}
inline void RemoveHiddenGames(std::vector<GameInfo>& games, const std::vector<std::string>& hidden)
{
    if (hidden.empty()) return;
    games.erase(std::remove_if(games.begin(), games.end(), [&](const GameInfo& game) {
        return MatchesLibraryIds(game, hidden);
    }), games.end());
}
inline void MergeManualGames(std::vector<GameInfo>& games, const std::vector<PublicGame>& manual)
{
    for (const auto& item : manual)
        if (std::none_of(games.begin(), games.end(), [&](const auto& game) { return SameGame(item, game); }))
            games.push_back(ManualGameInfo(item));
}
} // namespace nxnow::library
