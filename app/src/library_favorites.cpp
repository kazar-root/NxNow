#include "library_favorites.hpp"
#include "app_paths.hpp"
#include "atomic_file_replace.hpp"

#include <jansson.h>
#include <memory>

namespace nxnow::library
{
namespace
{
using Json = std::unique_ptr<json_t, decltype(&json_decref)>;
Json Read()
{
    for (const auto& suffix : {"", ".bak"})
    {
        Json root(json_load_file((AppHomePath() + "/favorites.json" + suffix).c_str(), 0, nullptr), json_decref);
        if (root && json_is_object(root.get()) && json_is_object(json_object_get(root.get(), "users")))
            return root;
    }
    Json root(json_object(), json_decref);
    json_object_set_new(root.get(), "version", json_integer(1));
    json_object_set_new(root.get(), "users", json_object());
    return root;
}
Favorites Decode(json_t* array)
{
    Favorites result;
    size_t index;
    json_t* entry;
    json_array_foreach(array, index, entry)
        if (json_is_string(entry) && json_string_length(entry) > 0)
            result.insert(json_string_value(entry));
    return result;
}
}
Favorites LoadFavorites(const std::string& user_id)
{
    if (user_id.empty()) return {};
    const auto root = Read();
    return Decode(json_object_get(json_object_get(root.get(), "users"), user_id.c_str()));
}
bool SetFavorite(const std::string& user_id, const std::string& game_id, bool favorite)
{
    if (user_id.empty() || game_id.empty()) return false;
    PrepareAppStorage();
    auto root = Read();
    auto* users = json_object_get(root.get(), "users");
    auto entries = Decode(json_object_get(users, user_id.c_str()));
    if (favorite) entries.insert(game_id); else entries.erase(game_id);
    auto* array = json_array();
    for (const auto& id : entries) json_array_append_new(array, json_string(id.c_str()));
    json_object_set_new(users, user_id.c_str(), array);
    const auto destination = AppHomePath() + "/favorites.json";
    const auto temporary = destination + ".tmp";
    if (json_dump_file(root.get(), temporary.c_str(), JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        std::remove(temporary.c_str());
        return false;
    }
    return storage::ReplaceWithTemporaryFile(temporary, destination);
}
}
