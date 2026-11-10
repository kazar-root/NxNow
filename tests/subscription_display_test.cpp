#include "subscription_display.hpp"

#include <cassert>

int main()
{
    nxnow::SubscriptionInfo info;
    info.available = true;
    info.remaining_hours = 1.5;
    assert(nxnow::subscription::FormatTimeRemaining(info) == "1h 30m left");

    info.remaining_hours = 0.25;
    assert(nxnow::subscription::FormatTimeRemaining(info) == "15m left");

    info.is_unlimited = true;
    assert(nxnow::subscription::FormatTimeRemaining(info) == "Unlimited");

    info.has_storage = true;
    info.has_storage_usage = true;
    info.storage_size_gb = 100.0;
    info.storage_used_gb = 24.5;
    assert(nxnow::subscription::FormatStorageRemaining(info) == "75.5 GB left");
    assert(nxnow::subscription::FormatStorageUsage(info) == "24.5 GB used of 100 GB");

    return 0;
}
