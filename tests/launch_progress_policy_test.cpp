#include "launch_progress_policy.hpp"

#include <cassert>
#include <string_view>

int main()
{
    nxnow::SessionInfo info;
    auto progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 1 && progress.position == -1);
    info.queue_position = 123;
    progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 1 && progress.position == 123);
    info.status = 1;
    progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 1 && progress.position == 123);
    info.queue_position = 0;
    progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 2 && progress.position == -1);
    info.status = 0;
    progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 1 && progress.position == 0);
    info.app_patching = true;
    progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 2 && progress.position == -1);
    assert(std::string_view(progress.title) == "Updating the game");
    info.app_patching = false;
    info.ads_pending = true;
    progress = nxnow::DescribeAllocation(info);
    assert(progress.stage == 2 && progress.position == -1);
    assert(std::string_view(progress.title) == "Waiting for NVIDIA confirmation");
    info.ads_pending = false;
    info.status = 6;
    assert(nxnow::DescribeAllocation(info).stage == 2);
    info.status = 2;
    info.signaling_url = "fixture://ready";
    assert(info.ready_for_stream());
}
