#include "stream/DecodeQueuePolicy.hpp"

#include <cassert>

static_assert(nxnow::video::MaximumDecodeUnitBytes() == 2 * 1024 * 1024);
static_assert(nxnow::video::MaximumQueuedAccessUnits(30) == 4);
static_assert(nxnow::video::MaximumQueuedAccessUnits(60) == 8);
static_assert(nxnow::video::MaximumQueuedAccessUnits(120) == 8);
static_assert(nxnow::video::MaximumDecodeQueueDelayMs() == 67);

int main()
{
    assert(nxnow::video::MaximumQueuedAccessUnits(1) == 2);
    assert(nxnow::video::MaximumQueuedAccessUnits(60) == 8);
    return 0;
}
