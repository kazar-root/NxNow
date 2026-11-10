#include "gfn_client.hpp"
#include "manual_library_policy.hpp"
#include "app_paths.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace { std::string directory; }
namespace nxnow {
GfnClient::GfnClient() = default;
const std::string& AppHomePath() { return directory; }
void PrepareAppStorage() { std::filesystem::create_directories(directory); }
}
int main()
{
    auto path = (std::filesystem::temp_directory_path() / "nxnow-manual-XXXXXX").string();
    assert(mkdtemp(path.data())); directory = path;
    nxnow::GfnClient client;
    nxnow::AuthSession first; first.user.user_id = "first"; first.provider.idp_id = "NVIDIA";
    nxnow::AuthSession other = first; other.user.user_id = "other";
    nxnow::PublicGame game; game.id = "catalog-uuid"; game.uuid = game.id;
    game.launch_app_id = "12345"; game.title = "Jeu possédé"; game.store = "STEAM";
    game.description = "Description française"; game.description_language = "fr";
    game.variants = {{"12345", "STEAM", "NOT_OWNED", "", "AVAILABLE", false}};
    assert(client.AddManualLibraryGame(first, game));
    assert(client.AddManualLibraryGame(first, game));
    const auto loaded = client.LoadManualLibraryGames(first);
    assert(loaded.size() == 1 && loaded[0].title == game.title);
    assert(loaded[0].description_language == "fr" && loaded[0].variants[0].id == "12345");
    assert(loaded[0].is_in_library);
    assert(client.LoadManualLibraryGames(other).empty());
    other.user.user_id = "first"; other.provider.idp_id = "ANOTHER_PROVIDER";
    assert(client.LoadManualLibraryGames(other).empty());
    std::vector<nxnow::GameInfo> games;
    nxnow::library::MergeManualGames(games, loaded);
    assert(games.size() == 1 && games[0].launch_app_id == "12345");
    // A later store sync replaces the declaration without duplicating its variant.
    games[0].id = "live-id"; games[0].uuid = "live-uuid";
    nxnow::library::MergeManualGames(games, loaded);
    assert(games.size() == 1 && games[0].id == "live-id");
    std::filesystem::copy_file(directory + "/manual_library.json", directory + "/manual_library.json.bak");
    std::ofstream(directory + "/manual_library.json") << "interrupted write";
    assert(client.LoadManualLibraryGames(first).size() == 1);
    first.reauthentication_required = true;
    assert(!client.AddManualLibraryGame(first, game));
    std::filesystem::remove_all(directory);
}
