#include "stream/DecodeQueuePolicy.hpp"

#include <cassert>

static_assert(nxnow::video::MaximumDecodeUnitBytes() == 2 * 1024 * 1024);
static_assert(nxnow::video::MaximumQueuedAccessUnits(30) == 3);
static_assert(nxnow::video::MaximumQueuedAccessUnits(60) == 5);
static_assert(nxnow::video::MaximumQueuedAccessUnits(120) == 6);
static_assert(nxnow::video::MaximumDecodeQueueDelayMs(30) == 100);
static_assert(nxnow::video::MaximumDecodeQueueDelayMs(60) == 83);
static_assert(nxnow::video::MaximumDecodeQueueDelayMs(120) == 67);
static_assert(nxnow::video::ReadyFrameQueueCapacity() == 3);
static_assert(nxnow::video::StartupReadyFrameDepth() == 2);
static_assert(nxnow::video::FrameWaitBudgetMicroseconds(60) == 6500);
static_assert(nxnow::video::FrameWaitBudgetMicroseconds(120) == 3333);
static_assert(nxnow::video::FrameWaitBudgetMicroseconds(240) == 2500);

int main()
{
    assert(nxnow::video::MaximumQueuedAccessUnits(0) == 2);
    assert(nxnow::video::MaximumQueuedAccessUnits(1) == 2);
    assert(nxnow::video::MaximumDecodeQueueDelayMs(0) == 134);
    assert(nxnow::video::FrameWaitBudgetMicroseconds(0) == 6500);
    return 0;
}
