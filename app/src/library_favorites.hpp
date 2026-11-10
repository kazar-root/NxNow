#pragma once

#include <set>
#include <string>

namespace nxnow::library
{
using Favorites = std::set<std::string>;
Favorites LoadFavorites(const std::string& user_id);
bool SetFavorite(const std::string& user_id, const std::string& game_id, bool favorite);
}
