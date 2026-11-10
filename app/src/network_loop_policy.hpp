#pragma once

namespace nxnow::network
{

constexpr int LoopBackoffMilliseconds(bool ready, int processed_datagrams)
{
    if (processed_datagrams > 0)
        return 0;
    return ready ? 1 : 2;
}

} // namespace nxnow::network
