#pragma once

#include "models.hpp"

namespace nxnow
{
struct AllocationProgress
{
    int stage;
    const char* title;
    const char* detail;
    int position = -1;
};

inline AllocationProgress DescribeAllocation(const SessionInfo& info)
{
    if (info.app_patching)
        return {2, "Updating the game", "NVIDIA is updating this game. Launch will resume automatically."};
    if (info.ads_pending || info.status == 6)
        return {2, "Waiting for NVIDIA confirmation", "NVIDIA must finish its confirmation before the game can start."};
    if (info.status == 0 || (info.status == 1 && info.queue_position > 0))
        return {1, "Waiting in queue...",
            "Your game starts automatically when a cloud rig is available.", info.queue_position};
    return {2, "Preparing your cloud rig", "Your place is ready. GeForce NOW is starting your game."};
}
}
