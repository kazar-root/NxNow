#pragma once

#include <string>

namespace nxnow
{

void ResetNteAutoLoginLog(const std::string& session_context);
void AppendNteAutoLoginLog(const std::string& line);

} // namespace nxnow
