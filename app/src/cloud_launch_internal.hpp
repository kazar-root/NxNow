#pragma once

#include "gfn_client.hpp"
#include "stream_settings.hpp"
#include "deferred_keyboard_input.hpp"
#include <memory>
#include <string>

namespace nxnow
{
void PresentCloudStream(const SessionInfo& info, const GfnClient& client,
                        const AuthSession& auth, const std::string& title,
                        const StreamSettings& settings, const std::string& image_url = {},
                        const std::string& launch_store = {},
                        std::shared_ptr<input::DeferredKeyboardInput> pending_keyboard = {});
}
