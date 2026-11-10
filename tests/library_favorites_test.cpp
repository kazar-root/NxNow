#include "app_paths.hpp"
#include "library_favorites.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace { std::string home; }
namespace nxnow
{
const std::string& AppHomePath() { return home; }
void PrepareAppStorage() { std::filesystem::create_directories(home); }
}
int main()
{
    auto pattern = (std::filesystem::temp_directory_path() / "nxnow-favorites-XXXXXX").string();
    const char* temporary = mkdtemp(pattern.data()); assert(temporary); home = temporary;
    using namespace nxnow::library;
    assert(LoadFavorites("alice").empty());
    assert(!SetFavorite("", "game-1", true));
    assert(!SetFavorite("alice", "", true));
    assert(SetFavorite("alice", "game-1", true));
    assert(SetFavorite("bob", "game-2", true));
    assert(SetFavorite("alice", "game-1", true));
    assert(LoadFavorites("alice") == Favorites{"game-1"});
    assert(LoadFavorites("bob") == Favorites{"game-2"});
    assert(SetFavorite("alice", "game-1", false));
    assert(LoadFavorites("alice").empty());
    assert(LoadFavorites("bob").contains("game-2"));
    std::filesystem::copy_file(home + "/favorites.json", home + "/favorites.json.bak",
        std::filesystem::copy_options::overwrite_existing);
    std::ofstream(home + "/favorites.json") << "{";
    assert(LoadFavorites("bob").contains("game-2"));
    assert(SetFavorite("alice", "game-3", true));
    assert(LoadFavorites("alice").contains("game-3"));
    assert(LoadFavorites("bob").contains("game-2"));
    assert(!std::filesystem::exists(home + "/favorites.json.tmp"));
    std::filesystem::remove_all(home);
}
