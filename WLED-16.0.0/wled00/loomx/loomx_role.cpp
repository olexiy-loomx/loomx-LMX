#include "loomx_role.h"
#include <Preferences.h>

namespace Loomx {

  // NVS namespace is limited to 15 chars; key is limited to 15 chars.
  static constexpr const char* NVS_NS  = "loomx";
  static constexpr const char* NVS_KEY = "role";

  Role getRole() {
    Preferences p;
    if (!p.begin(NVS_NS, true /*read-only*/)) return Role::Unprovisioned;
    uint8_t raw = p.getUChar(NVS_KEY, static_cast<uint8_t>(Role::Unprovisioned));
    p.end();
    if (raw > static_cast<uint8_t>(Role::Child)) return Role::Unprovisioned;
    return static_cast<Role>(raw);
  }

  bool setRole(Role r) {
    Preferences p;
    if (!p.begin(NVS_NS, false /*read-write*/)) return false;
    bool ok = p.putUChar(NVS_KEY, static_cast<uint8_t>(r)) > 0;
    p.end();
    return ok;
  }

  const char* roleName(Role r) {
    switch (r) {
      case Role::Parent:        return "Parent";
      case Role::Child:         return "Child";
      case Role::Unprovisioned: return "Unprovisioned";
    }
    return "Unknown";
  }

}
