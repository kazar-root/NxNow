#include "membership_tier_policy.hpp"

#include <cassert>

int main()
{
    using nxnow::membership::DisplayLabel;
    assert(DisplayLabel("ULTIMATE", true) == "ULTIMATE");
    assert(DisplayLabel("FREE", true) == "FREE");
    assert(DisplayLabel("FREE", false) == "Membership unavailable");
    assert(DisplayLabel("", false) == "Membership unavailable");
    assert(nxnow::membership::AccentForTier("ULTIMATE") ==
           nxnow::membership::TierAccent::Ultimate);
    assert(nxnow::membership::AccentForTier("ultimate") ==
           nxnow::membership::TierAccent::Ultimate);
    assert(nxnow::membership::AccentForTier("PERFORMANCE") ==
           nxnow::membership::TierAccent::Performance);
    assert(nxnow::membership::AccentForTier("PRIORITY") ==
           nxnow::membership::TierAccent::Performance);
    assert(nxnow::membership::AccentForTier("FREE") ==
           nxnow::membership::TierAccent::Neutral);
    return 0;
}
