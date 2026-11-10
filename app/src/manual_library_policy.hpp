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
inline void MergeManualGames(std::vector<GameInfo>& games, const std::vector<PublicGame>& manual)
{
    for (const auto& item : manual)
        if (std::none_of(games.begin(), games.end(), [&](const auto& game) { return SameGame(item, game); }))
            games.push_back(ManualGameInfo(item));
}
} // namespace nxnow::library
