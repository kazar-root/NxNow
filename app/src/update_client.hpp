#pragma once
#include "http_client.hpp"
#include <string>
namespace nxnow
{
enum class UpdateStatus { Current, Available, Ahead, NoRelease };
struct UpdateResult
{
    UpdateStatus status;
    std::string latest_version;
    std::string release_url;
};
UpdateResult ParseUpdateResponse(const HttpResponse& response, const std::string& installed_version);
UpdateResult CheckLatestRelease(const std::string& installed_version);
}
