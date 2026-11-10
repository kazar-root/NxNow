#pragma once

#include <borealis.hpp>

#include <string>
#include <vector>

namespace nxnow::ui
{

brls::Button* MakeGameBrowserActionButton(const std::string& text);
brls::Box* MakeGameBrowserHeader(
    const std::string& title, const std::vector<brls::View*>& actions);

} // namespace nxnow::ui
