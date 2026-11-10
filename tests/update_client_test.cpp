#include "update_client.hpp"
#include "app_version.hpp"
#include <cassert>
#include <stdexcept>

namespace { nxnow::HttpResponse response; }
namespace nxnow {
HttpResponse HttpClient::Get(const std::string& url, const std::string& agent,
    const std::vector<std::string>& headers, const std::string& proxy, HttpTransferControl control) const
{
    assert(url == kLatestReleaseApi && agent == "NxNow/0.1" && proxy.empty());
    assert(control.max_body_bytes == 256 * 1024 && headers.size() == 2);
    return response;
}
}
int main()
{
    using namespace nxnow;
    const auto release = [](std::string tag) {
        return HttpResponse{200, "{\"tag_name\":\"" + tag + "\",\"html_url\":\"https://github.com/kazar-root/NxNow/releases/tag/" + tag + "\"}"};
    };
    response = release("v0.1");
    assert(CheckLatestRelease("0.1").status == UpdateStatus::Current);
    assert(ParseUpdateResponse(release("v0.1.0"), "0.1").status == UpdateStatus::Current);
    assert(ParseUpdateResponse(release("v0.1.1"), "0.1").status == UpdateStatus::Available);
    assert(ParseUpdateResponse(release("v0.10"), "0.9").status == UpdateStatus::Available);
    assert(ParseUpdateResponse(release("v0.1"), "0.2").status == UpdateStatus::Ahead);
    assert(ParseUpdateResponse({404, "{}"}, "0.1").status == UpdateStatus::NoRelease);
    for (const auto& bad : {HttpResponse{403, "{}"}, HttpResponse{200, "invalid JSON"},
         release("v0.1.0-beta"), release("v0.1."), HttpResponse{200,
         R"({"tag_name":"v9.0","html_url":"https://other.example/release"})"}})
    {
        bool rejected = false;
        try { (void)ParseUpdateResponse(bad, "0.1"); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
}
