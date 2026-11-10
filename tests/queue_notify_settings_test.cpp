#include "app_paths.hpp"
#include "stream_settings.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
std::string home;
}

namespace nxnow
{
const std::string& AppHomePath() { return home; }
void PrepareAppStorage() { std::filesystem::create_directories(home); }
}

int main()
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "nxnow-queue-settings-XXXXXX").string();
    char* directory = mkdtemp(pattern.data());
    assert(directory);
    home = directory;
    assert(nxnow::LoadStreamSettings().queue_notify_threshold == 10);
    for (int threshold : {5, 10, 20, 50}) {
        auto settings = nxnow::LoadStreamSettings();
        settings.queue_notify_threshold = threshold;
        assert(nxnow::SaveStreamSettings(settings));
        assert(nxnow::LoadStreamSettings().queue_notify_threshold == threshold);
    }
    for (const std::string value : {"0", "-1", "15", "2147483648", "null", "\"5\""}) {
        {
            std::ofstream file(home + "/stream_settings.json");
            file << "{\"queue_notify_threshold\":" << value << "}";
            file.close();
            assert(file.good());
        }
        assert(nxnow::LoadStreamSettings().queue_notify_threshold == 10);
    }
    std::filesystem::remove_all(home);
}
