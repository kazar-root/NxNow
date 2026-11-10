#include "../app/src/gfn/catalog.cpp"

#include <cassert>

namespace
{
void Reject(const std::string& payload)
{
    auto root = nxnow::gfn::detail::LoadJson(payload);
    bool rejected = false;
    try
    {
        nxnow::ParseCatalogPage(root, "previous");
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    assert(rejected);
}
}

int main()
{
    auto free_entry = nxnow::gfn::detail::LoadJson(R"({"id":100013311,"title":"Fortnite","status":"AVAILABLE","store":"Epic","genres":["Action","Free To Play"]})");
    const auto free_game = nxnow::ParsePublicGame(free_entry.get());
    assert(free_game.id == "100013311" && free_game.launch_app_id == free_game.id);
    assert(free_game.is_free_to_play && free_game.title == "Fortnite");
    json_object_set_new(free_entry.get(), "genres", json_pack("[s]", "Action"));
    assert(!nxnow::ParsePublicGame(free_entry.get()).is_free_to_play);
    json_object_set_new(free_entry.get(), "status", json_string("UNAVAILABLE"));
    assert(nxnow::ParsePublicGame(free_entry.get()).id.empty());
    const auto request = nxnow::gfn::detail::LoadJson(
        nxnow::BuildCatalogRequestBody("GFN-PC", "Example", "cursor-60"));
    json_t* variables = json_object_get(request.get(), "variables");
    assert(json_integer_value(json_object_get(variables, "fetchCount")) == 10);
    assert(nxnow::gfn::detail::GetString(variables, "cursor") == "cursor-60");
    assert(nxnow::gfn::detail::GetString(variables, "searchString") == "Example");
    const auto library_request = nxnow::gfn::detail::LoadJson(
        nxnow::BuildLibraryRequestBody("GFN-PC", "cursor-120"));
    json_t* library_variables = json_object_get(library_request.get(), "variables");
    assert(json_integer_value(json_object_get(library_variables, "fetchCount")) == 60);
    assert(nxnow::gfn::detail::GetString(library_variables, "cursor") == "cursor-120");
    json_t* library_status = json_object_get(json_object_get(json_object_get(json_object_get(
        json_object_get(library_variables, "filters"), "variants"), "gfn"), "library"), "status");
    assert(nxnow::gfn::detail::GetString(library_status, "notEquals") == "NOT_OWNED");
    assert(nxnow::gfn::detail::GetString(library_request.get(), "query").find("after: $cursor") !=
           std::string::npos);

    for (const auto& [language, locale] : std::vector<std::pair<std::string, std::string>> {
        {"fr", "fr_FR"}, {"en", "en_US"}, {"es", "es_ES"}, {"it", "it_IT"},
        {"ru", "ru_RU"}, {"uk", "uk_UA"}, {"pl", "pl_PL"}, {"zh-CN", "zh_CN"}})
    {
        const auto localized = nxnow::gfn::detail::LoadJson(nxnow::BuildLibraryRequestBody("GFN-PC", "", language));
        assert(nxnow::gfn::detail::GetString(json_object_get(localized.get(), "variables"), "locale") == locale);
        const auto search = nxnow::gfn::detail::LoadJson(nxnow::BuildCatalogRequestBody("GFN-PC", "Example", "", language));
        assert(nxnow::gfn::detail::GetString(json_object_get(search.get(), "variables"), "locale") == locale);
    }
    assert(nxnow::catalog::ImageUrl("https://other.example/cover", 224) == "https://other.example/cover");
    auto descriptions = nxnow::gfn::detail::LoadJson(R"({"id":"fr","title":"Jeu","description":"moyen", "shortDescription":"bref", "longDescription":"Description complète en français."})");
    auto french = nxnow::ParseApp(descriptions.get());
    french.description_language = "fr";
    assert(french.description == "Description complète en français.");
    assert(nxnow::ToPublicGame(french).description == french.description);
    assert(nxnow::ToPublicGame(french).description_language == "fr");
    json_object_del(descriptions.get(), "longDescription");
    assert(nxnow::ParseApp(descriptions.get()).description == "moyen");
    json_object_del(descriptions.get(), "description");
    assert(nxnow::ParseApp(descriptions.get()).description == "bref");

    auto library_page = nxnow::gfn::detail::LoadJson(R"({"data":{"apps":{
        "items":[{"id":"owned","title":"Owned",
            "variants":[{"id":"456","appStore":"STEAM",
                "gfn":{"library":{"status":"PLATFORM_SYNC","lastPlayedDate":"2026-10-01T10:00:00Z"}}}]}],
        "pageInfo":{"hasNextPage":true,"endCursor":"cursor-180"}}}})");
    const auto library_result = nxnow::ParseAppsPage(library_page, "cursor-120");
    assert(library_result.next_cursor == "cursor-180");
    assert(library_result.games.size() == 1);
    assert(library_result.games[0].is_in_library);
    assert(library_result.games[0].last_played == "2026-10-01T10:00:00Z");
    assert(library_result.games[0].launch_app_id == "456");

    Reject(R"({})");
    Reject(R"({"data":{"apps":{"items":[]}}})");
    Reject(R"({"data":{"apps":{"items":{},"pageInfo":{"hasNextPage":false}}}})");
    Reject(R"({"data":{"apps":{"items":[],"pageInfo":{"hasNextPage":"false"}}}})");
    Reject(R"({"data":{"apps":{"items":[],"pageInfo":{"hasNextPage":true}}}})");
    Reject(R"({"data":{"apps":{"items":[],"pageInfo":{"hasNextPage":true,"endCursor":"previous"}}}})");
    Reject(R"({"errors":[{"message":"Catalog unavailable"}]})");
    std::string oversized = R"({"data":{"apps":{"items":[)";
    for (int index = 0; index < 11; ++index)
        oversized += index == 0 ? "{}" : ",{}";
    oversized += R"(],"pageInfo":{"hasNextPage":false}}}})";
    Reject(oversized);

    auto empty = nxnow::gfn::detail::LoadJson(
        R"({"data":{"apps":{"items":[],"pageInfo":{"hasNextPage":false}}}})");
    const auto empty_page = nxnow::ParseCatalogPage(empty, "old");
    assert(empty_page.games.empty());
    assert(!empty_page.next_cursor && !empty_page.total_count);

    auto page = nxnow::gfn::detail::LoadJson(R"({"data":{"apps":{
        "items":[null, {"id":"broken"}, {"id":"game","title":"Example",
            "images":{"KEY_ART":"https://img.nvidiagrid.net/example"},
            "variants":[{"id":"123","appStore":"STEAM"}]}],
        "pageInfo":{"hasNextPage":true,"endCursor":"next","totalCount":500}}}})");
    const auto result = nxnow::ParseCatalogPage(page, "");
    assert(result.next_cursor == "next" && result.total_count == 500);
    assert(result.games.size() == 1);
    assert(result.games[0].title == "Example");
    assert(result.games[0].launch_app_id == "123");
    assert(result.games[0].image_url == "https://img.nvidiagrid.net/example;w=256");
    auto artwork = nxnow::gfn::detail::LoadJson(R"({"id":"art","title":"Real art",
        "images":{"KEY_ART":"https://img.nvidiagrid.net/key","GAME_BOX_ART":"https://img.nvidiagrid.net/box",
        "HERO_IMAGE":"https://img.nvidiagrid.net/hero","TV_BANNER":"https://img.nvidiagrid.net/banner"}})");
    const auto parsed = nxnow::ParseApp(artwork.get());
    assert(parsed.image_url == "https://img.nvidiagrid.net/key;w=256");
    assert(parsed.portrait_url == "https://img.nvidiagrid.net/box;w=224");
    assert(parsed.background_url == "https://img.nvidiagrid.net/hero;w=640");
    nxnow::GameInfo merged;
    nxnow::MergeGame(merged, parsed);
    assert(merged.portrait_url == parsed.portrait_url && merged.background_url == parsed.background_url);
}
