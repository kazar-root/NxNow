#include "network_loop_policy.hpp"

#include <cassert>

static_assert(nxnow::network::LoopBackoffMilliseconds(false, 0) == 2);
static_assert(nxnow::network::LoopBackoffMilliseconds(true, 0) == 1);
static_assert(nxnow::network::LoopBackoffMilliseconds(true, 1) == 0);
static_assert(nxnow::network::LoopBackoffMilliseconds(true, 24) == 0);

int main()
{
    using nxnow::network::LoopBackoffMilliseconds;
    assert(LoopBackoffMilliseconds(false, 0) > LoopBackoffMilliseconds(true, 0));
    assert(LoopBackoffMilliseconds(true, 8) == 0);
    return 0;
}
