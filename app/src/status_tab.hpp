#pragma once

#include <borealis.hpp>

namespace nxnow
{

class StatusTab : public brls::Box
{
  public:
    StatusTab();

  private:
    bool OpenArchitectureDialog(brls::View* view);
};

} // namespace nxnow
