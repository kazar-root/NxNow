#pragma once

#include <chrono>

namespace nxnow::ui
{

constexpr bool ShouldRefreshStatus(
    std::chrono::milliseconds elapsed, bool has_displayed_status)
{
    return !has_displayed_status || elapsed >= std::chrono::seconds(1);
}

} // namespace nxnow::ui
