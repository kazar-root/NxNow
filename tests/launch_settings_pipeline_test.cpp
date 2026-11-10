#include "app_paths.hpp"
#include "gfn/cloud_session_internal.hpp"
#include "launch_settings_policy.hpp"
#include "webrtc/nvst_sdp.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace
{
std::string home;
nxnow::StreamSettings expected;
int launches = 0;
int network_tests = 0;
const std::string selected_region = "https://paris.fixture.invalid/";
}

namespace nxnow
{
const std::string& AppHomePath() { return home; }
void PrepareAppStorage() { std::filesystem::create_directories(home); }
bool StreamDiagnosticsEnabled() { return false; }
AuthSession GfnClient::EnsureFreshSavedSession(const AuthSession& auth) const { return auth; }
AuthSession GfnClient::ForceRefreshSavedSession(const AuthSession& auth) const { return auth; }
std::vector<StreamRegion> GfnClient::FetchStreamRegions(AuthSession&) const
{
    // A manual region must not be replaced by a latency-selected endpoint.
    assert(false);
    return {};
}
std::vector<StreamRegion> GfnClient::MeasureStreamRegionLatencies(std::vector<StreamRegion> regions) const { return regions; }

HttpResponse HttpClient::Request(const std::string& method, const std::string& url,
    const std::string&, const std::vector<std::string>&, const std::string& body,
    const std::string&, HttpTransferControl) const
{
    assert(url.starts_with(selected_region));
    if (method == "DELETE")
        return {204, ""};
    assert(method == "POST");
    const auto root = gfn::detail::LoadJson(body);
    if (url.ends_with("v2/nettestsession"))
    {
        ++network_tests;
        auto* profile = json_object_get(json_object_get(root.get(), "netTestRequestData"), "netTestProfile");
        assert(gfn::detail::GetInteger(profile, "widthInPixels") == expected.width);
        assert(gfn::detail::GetInteger(profile, "heightInPixels") == expected.height);
        assert(gfn::detail::GetInteger(profile, "framesPerSecond") == expected.fps);
        return {200, R"({"requestStatus":{"statusCode":1},"netTestSession":{"sessionId":"synthetic-nettest"}})"};
    }
    ++launches;
    assert(url.ends_with("languageCode=" + expected.game_language));
    assert(url.find("languageCode=Auto") == std::string::npos);
    assert(url.find("keyboardLayout=" + expected.keyboard_layout) != std::string::npos);
    auto* request = json_object_get(root.get(), "sessionRequestData");
    assert(gfn::detail::GetInteger(request, "remoteControllersBitmap") == 1);
    assert(gfn::detail::GetInteger(request, "appLaunchMode") == 2);
    auto* monitor = json_array_get(json_object_get(request, "clientRequestMonitorSettings"), 0);
    assert(gfn::detail::GetInteger(monitor, "widthInPixels") == expected.width);
    assert(gfn::detail::GetInteger(monitor, "heightInPixels") == expected.height);
    assert(gfn::detail::GetInteger(monitor, "framesPerSecond") == expected.fps);
    assert(gfn::detail::GetInteger(json_object_get(request, "requestedStreamingFeatures"), "maxBitrateKbps") == expected.bitrate_kbps);
    assert(json_is_true(json_object_get(request, "enablePersistingInGameSettings")) == expected.persist_game_settings);
    return {200, R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"synthetic-seat","status":1}})"};
}
HttpResponse HttpClient::Get(const std::string& url, const std::string& ua,
    const std::vector<std::string>& headers, const std::string& proxy, HttpTransferControl control) const
{ return Request("GET", url, ua, headers, {}, proxy, control); }
HttpResponse HttpClient::Post(const std::string& url, const std::string& ua,
    const std::vector<std::string>& headers, const std::string& body, const std::string& proxy, HttpTransferControl control) const
{ return Request("POST", url, ua, headers, body, proxy, control); }
}

int main()
{
    namespace fs = std::filesystem;
    const auto previous = fs::current_path();
    auto pattern = (fs::temp_directory_path() / "nxnow-launch-settings-XXXXXX").string();
    const char* directory = mkdtemp(pattern.data());
    assert(directory);
    fs::current_path(directory);
    home = std::string(directory) + "/settings";
    fs::create_directories(nxnow::gfn::detail::GetAppHome());
    nxnow::AuthSession auth;
    auth.user.user_id = "synthetic-user";
    auth.tokens.id_token = "synthetic-token";
    auth.provider.streaming_service_url = "https://other.fixture.invalid/";
    nxnow::GfnClient client;
    for (const char* store : {"Steam", "EA"})
    for (bool custom : {false, true})
    {
        for (const char* override : {"Auto", "en_GB", "fr_FR"})
        {
            nxnow::StreamSettings saved;
            saved.interface_language = "fr";
            saved.game_language = override;
            saved.region = selected_region;
            saved.preset_id = custom ? "custom" : "balanced";
            saved.width = custom ? 1920 : 1280;
            saved.height = custom ? 1080 : 720;
            saved.fps = custom ? 30 : 60;
            saved.bitrate_kbps = custom ? 25000 : 12000;
            saved.persist_game_settings = custom;
            assert(nxnow::SaveStreamSettings(saved));
            const auto read_back = nxnow::LoadStreamSettings();
            assert(read_back == saved);
            expected = nxnow::settings::ResolveLaunchSettings(read_back);
            assert(expected.game_language == (std::string(override) == "Auto" ? "fr_FR" : override));
            const auto info = client.StartSession(auth, "123", read_back, store, "fixture-game");
            const auto sdp = nxnow::webrtc::BuildNvstSdp("v=0\na=ice-ufrag:fixture\n", expected, {});
            for (const auto& line : {
                "a=video.clientViewportWd:" + std::to_string(expected.width),
                "a=video.clientViewportHt:" + std::to_string(expected.height),
                "a=video.maxFPS:" + std::to_string(expected.fps),
                "a=vqos.bw.maximumBitrateKbps:" + std::to_string(expected.bitrate_kbps)})
                assert(sdp.find(line + "\n") != std::string::npos);
            // Persisting the next choice must not mutate this launched session.
            auto next = read_back;
            next.bitrate_kbps = 8000;
            next.game_language = "it_IT";
            assert(nxnow::SaveStreamSettings(next));
            assert(expected.bitrate_kbps == saved.bitrate_kbps);
            assert(nxnow::LoadStreamSettings().game_language == "it_IT");
            client.StopSession(auth, info.session_id);
        }
    }
    assert(launches == 12 && network_tests == 12);
    fs::current_path(previous);
    fs::remove_all(directory);
}
