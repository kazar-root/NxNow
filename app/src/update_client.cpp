#include "update_client.hpp"
#include "app_version.hpp"
#include <jansson.h>
#include <array>
#include <charconv>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace nxnow
{
namespace
{
std::array<unsigned, 3> Version(std::string_view value)
{
    if (value.starts_with('v')) value.remove_prefix(1);
    std::array<unsigned, 3> result {};
    size_t count = 0;
    while (!value.empty() && count < result.size())
    {
        const auto end = value.find('.');
        const auto part = value.substr(0, end);
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), result[count++]);
        if (part.empty() || parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size())
            throw std::runtime_error("Unrecognized release version");
        if (end == std::string_view::npos) { value = {}; break; }
        value.remove_prefix(end + 1);
        if (value.empty()) throw std::runtime_error("Unrecognized release version");
    }
    if (count < 2 || !value.empty()) throw std::runtime_error("Unrecognized release version");
    return result;
}
}
UpdateResult ParseUpdateResponse(const HttpResponse& response, const std::string& installed_version)
{
    if (response.status_code == 404) return {UpdateStatus::NoRelease, {}, kProjectUrl};
    if (response.status_code != 200) throw std::runtime_error("GitHub update request failed");
    std::unique_ptr<json_t, decltype(&json_decref)> root(
        json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, nullptr), json_decref);
    if (!root) throw std::runtime_error("Invalid GitHub release response");
    const char* tag = json_string_value(json_object_get(root.get(), "tag_name"));
    const char* url = json_string_value(json_object_get(root.get(), "html_url"));
    if (!tag || !url || json_is_true(json_object_get(root.get(), "draft")) ||
        json_is_true(json_object_get(root.get(), "prerelease")) ||
        !std::string_view(url).starts_with(std::string(kProjectUrl) + "/releases/tag/"))
        throw std::runtime_error("Invalid GitHub release response");
    const auto installed = Version(installed_version), latest = Version(tag);
    return {installed < latest ? UpdateStatus::Available : installed == latest ? UpdateStatus::Current : UpdateStatus::Ahead,
            tag, url};
}
UpdateResult CheckLatestRelease(const std::string& installed_version)
{
    HttpClient client;
    return ParseUpdateResponse(client.Get(kLatestReleaseApi, std::string("NxNow/") + installed_version,
        {"Accept: application/vnd.github+json", "X-GitHub-Api-Version: 2022-11-28"}, {},
        {.max_body_bytes = 256 * 1024}), installed_version);
}
}
