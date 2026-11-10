#include "app_paths.hpp"
#include <sys/stat.h>

namespace nxnow
{
const std::string& AppHomePath()
{
    static const std::string path = "sdmc:/switch/NXNow";
    return path;
}

void PrepareAppStorage()
{
#ifdef __SWITCH__
    mkdir("sdmc:/switch", 0777);
    mkdir(AppHomePath().c_str(), 0777);
#endif
}
}
