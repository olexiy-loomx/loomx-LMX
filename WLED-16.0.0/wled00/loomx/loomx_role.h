#pragma once

#include <stdint.h>

namespace Loomx {

  enum class Role : uint8_t {
    Unprovisioned = 0,
    Parent        = 1,
    Child         = 2,
  };

  Role        getRole();
  bool        setRole(Role r);
  const char* roleName(Role r);

}
