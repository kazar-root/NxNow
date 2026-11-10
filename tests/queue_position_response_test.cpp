#include "gfn/cloud_session_internal.hpp"
#include "launch_progress_policy.hpp"

#include <cassert>

namespace nxnow { bool StreamDiagnosticsEnabled() { return false; } }

int main()
{
    using nxnow::gfn::cloud_session::ParseSessionResponse;
    for (const auto& [body, position] : {
        std::pair{R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"fixture","status":0}})", -1},
        std::pair{R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"fixture","status":0,"queuePosition":0}})", 0},
        std::pair{R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"fixture","status":0,"queuePosition":123}})", 123},
        std::pair{R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"fixture","status":1},"queuePosition":7})", 7},
        std::pair{R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"fixture","status":0,"queuePosition":"12"}})", 12},
        std::pair{R"({"requestStatus":{"statusCode":1},"session":{"sessionId":"fixture","status":0,"queuePosition":"invalid"}})", -1}})
    {
        const auto info = ParseSessionResponse({200, body}, "fixture", "fixture", "https://fixture.invalid/");
        assert(info.queue_position == position);
        const auto display = nxnow::DescribeAllocation(info);
        assert(display.stage == 1 && display.position == position);
    }
}
