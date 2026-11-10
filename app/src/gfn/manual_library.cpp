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
    size_t index; json_t* existing;
    json_array_foreach(entries, index, existing)
        if (String(existing, "id") == game.id) return true;
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
    const auto destination = AppHomePath() + "/manual_library.json";
    const auto temporary = destination + ".tmp";
    if (json_dump_file(root.get(), temporary.c_str(), JSON_INDENT(2) | JSON_SORT_KEYS) != 0)
    {
        std::remove(temporary.c_str()); return false;
    }
    return storage::ReplaceWithTemporaryFile(temporary, destination);
}
} // namespace nxnow
