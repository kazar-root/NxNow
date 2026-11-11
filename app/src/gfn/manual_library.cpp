#include "../gfn_client.hpp"
#include "../app_paths.hpp"
#include "../atomic_file_replace.hpp"

#include <jansson.h>
#include <algorithm>
#include <cstdio>
#include <memory>

namespace nxnow
{
namespace
{
using Json = std::unique_ptr<json_t, decltype(&json_decref)>;
std::string String(json_t* object, const char* key)
{
    const auto* value = json_string_value(json_object_get(object, key));
    return value ? value : "";
}
Json Read()
{
    for (const auto& suffix : {"", ".bak"})
    {
        Json root(json_load_file((AppHomePath() + "/manual_library.json" + suffix).c_str(),
                                JSON_REJECT_DUPLICATES, nullptr), json_decref);
        if (root && json_is_object(json_object_get(root.get(), "users"))) return root;
    }
    return Json(json_pack("{s:i,s:{}}", "version", 1, "users"), json_decref);
}
std::string Scope(const AuthSession& session)
{
    return session.provider.idp_id + ":" + session.user.user_id;
}
void Set(json_t* object, const char* key, const std::string& value)
{
    json_object_set_new(object, key, json_string(value.c_str()));
}
bool Save(json_t* root)
{
    const auto destination = AppHomePath() + "/manual_library.json";
    const auto temporary = destination + ".tmp";
    if (json_dump_file(root, temporary.c_str(), JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        std::remove(temporary.c_str()); return false;
    }
    return storage::ReplaceWithTemporaryFile(temporary, destination);
}
// Hidden GeForce NOW library games, per account: {"hidden": {"<scope>": [ids]}}.
json_t* HiddenIds(json_t* root, const std::string& scope, bool create)
{
    auto* hidden = json_object_get(root, "hidden");
    if (!json_is_object(hidden))
    {
        if (!create) return nullptr;
        hidden = json_object(); json_object_set_new(root, "hidden", hidden);
    }
    auto* ids = json_object_get(hidden, scope.c_str());
    if (!json_is_array(ids))
    {
        if (!create) return nullptr;
        ids = json_array(); json_object_set_new(hidden, scope.c_str(), ids);
    }
    return ids;
}
bool Contains(const std::vector<std::string>& ids, const std::string& value)
{
    return !value.empty() && std::find(ids.begin(), ids.end(), value) != ids.end();
}
}

std::vector<PublicGame> GfnClient::LoadManualLibraryGames(const AuthSession& session) const
{
    if (session.user.user_id.empty()) return {};
    const auto root = Read();
    auto* entries = json_object_get(json_object_get(root.get(), "users"), Scope(session).c_str());
    std::vector<PublicGame> games;
    size_t index; json_t* entry;
    json_array_foreach(entries, index, entry)
    {
        if (games.size() >= 2000) break;
        PublicGame game;
        game.id = String(entry, "id"); game.uuid = String(entry, "uuid");
        game.title = String(entry, "title"); game.launch_app_id = String(entry, "launch_app_id");
        game.store = String(entry, "store"); game.publisher = String(entry, "publisher");
        game.image_url = String(entry, "image_url"); game.description = String(entry, "description");
        game.description_language = String(entry, "description_language");
        game.is_free_to_play = json_is_true(json_object_get(entry, "free_to_play"));
        game.is_in_library = true;
        size_t variant_index; json_t* item;
        json_array_foreach(json_object_get(entry, "variants"), variant_index, item)
        {
            GameVariant variant;
            variant.id = String(item, "id"); variant.store = String(item, "store");
            variant.library_status = "MANUAL";
            if (!variant.id.empty()) game.variants.push_back(std::move(variant));
        }
        if (!game.id.empty() && !game.title.empty()) games.push_back(std::move(game));
    }
    return games;
}

bool GfnClient::AddManualLibraryGame(const AuthSession& session, const PublicGame& game) const
{
    if (session.user.user_id.empty() || session.reauthentication_required ||
        game.id.empty() || game.title.empty()) return false;
    PrepareAppStorage();
    auto root = Read();
    auto* users = json_object_get(root.get(), "users");
    const auto scope = Scope(session);
    auto* entries = json_object_get(users, scope.c_str());
    if (!json_is_array(entries))
    {
        entries = json_array(); json_object_set_new(users, scope.c_str(), entries);
    }
    // Adding a game again also shows it again if it had been removed.
    bool unhidden = false;
    if (auto* hidden = HiddenIds(root.get(), scope, false))
        for (size_t i = json_array_size(hidden); i-- > 0;)
        {
            const char* id = json_string_value(json_array_get(hidden, i));
            if (id && (id == game.id || id == game.uuid ||
                       (!game.launch_app_id.empty() && id == game.launch_app_id)))
            {
                json_array_remove(hidden, i); unhidden = true;
            }
        }
    size_t index; json_t* existing;
    json_array_foreach(entries, index, existing)
        if (String(existing, "id") == game.id) return !unhidden || Save(root.get());
    if (json_array_size(entries) >= 2000) return false;
    Json entry(json_object(), json_decref);
    Set(entry.get(), "id", game.id); Set(entry.get(), "uuid", game.uuid);
    Set(entry.get(), "title", game.title); Set(entry.get(), "launch_app_id", game.launch_app_id);
    Set(entry.get(), "store", game.store); Set(entry.get(), "publisher", game.publisher);
    Set(entry.get(), "image_url", game.image_url); Set(entry.get(), "description", game.description);
    Set(entry.get(), "description_language", game.description_language);
    json_object_set_new(entry.get(), "free_to_play", json_boolean(game.is_free_to_play));
    auto* variants = json_array();
    for (const auto& variant : game.variants)
        json_array_append_new(variants, json_pack("{s:s,s:s}", "id", variant.id.c_str(),
                                               "store", variant.store.c_str()));
    json_object_set_new(entry.get(), "variants", variants);
    json_array_append(entries, entry.get());
    return Save(root.get());
}

bool GfnClient::RemoveLibraryGame(const AuthSession& session, const std::vector<std::string>& game_ids) const
{
    std::vector<std::string> ids;
    for (const auto& id : game_ids)
        if (!id.empty() && !Contains(ids, id)) ids.push_back(id);
    if (session.user.user_id.empty() || ids.empty()) return false;
    PrepareAppStorage();
    auto root = Read();
    const auto scope = Scope(session);
    auto* entries = json_object_get(json_object_get(root.get(), "users"), scope.c_str());
    if (json_is_array(entries))
        for (size_t i = json_array_size(entries); i-- > 0;)
        {
            auto* entry = json_array_get(entries, i);
            if (Contains(ids, String(entry, "id")) || Contains(ids, String(entry, "uuid")) ||
                Contains(ids, String(entry, "launch_app_id")))
                json_array_remove(entries, i);
        }
    auto* hidden = HiddenIds(root.get(), scope, true);
    std::vector<std::string> already;
    size_t index; json_t* value;
    json_array_foreach(hidden, index, value)
        if (const char* id = json_string_value(value)) already.emplace_back(id);
    for (const auto& id : ids)
        if (!Contains(already, id) && json_array_size(hidden) < 4000)
            json_array_append_new(hidden, json_string(id.c_str()));
    return Save(root.get());
}

std::vector<std::string> GfnClient::LoadHiddenLibraryGameIds(const AuthSession& session) const
{
    std::vector<std::string> ids;
    if (session.user.user_id.empty()) return ids;
    const auto root = Read();
    auto* hidden = HiddenIds(root.get(), Scope(session), false);
    size_t index; json_t* value;
    json_array_foreach(hidden, index, value)
        if (const char* id = json_string_value(value)) ids.emplace_back(id);
    return ids;
}
} // namespace nxnow
