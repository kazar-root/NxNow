#include "app_paths.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

int main()
{
    namespace fs = std::filesystem;
    const auto previous = fs::current_path();
    std::string pattern = (fs::temp_directory_path() / "nxnow-migration-XXXXXX").string();
    const char* directory = mkdtemp(pattern.data());
    assert(directory);
    fs::current_path(directory);
    nxnow::PrepareAppStorage();
    assert(nxnow::AppHomePath() == "sdmc:/switch/NXNow");
    fs::create_directories(nxnow::AppHomePath() + "/nte");
    for (const char* file : {"auth_accounts.json", "auth_accounts.json.bak", "device_id.txt",
                            "stream_settings.json", "nte/login.txt"})
        std::ofstream(nxnow::AppHomePath() + "/" + file) << "synthetic";
    nxnow::PrepareAppStorage();
    for (const char* file : {"auth_accounts.json", "auth_accounts.json.bak", "device_id.txt",
                            "stream_settings.json", "nte/login.txt"})
    {
        std::string text;
        std::ifstream(nxnow::AppHomePath() + "/" + file) >> text;
        assert(text == "synthetic");
    }
    nxnow::PrepareAppStorage();
    assert(fs::is_directory(nxnow::AppHomePath()));
    fs::current_path(previous);
    fs::remove_all(directory);
}
