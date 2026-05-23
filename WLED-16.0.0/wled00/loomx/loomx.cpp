#include "loomx.h"
#include "loomx_role.h"
#include "loomx_parent.h"
#include "loomx_child.h"
#include "../wled.h"

#ifndef ARDUINO_ARCH_ESP32
#error "Loomx targets ESP32 only."
#endif

extern void handleLoomxActions();

namespace Loomx {

  void setup() {
    Role r = getRole();
    DEBUG_PRINTF_P(PSTR("[Loomx] boot role=%s\n"), roleName(r));
    switch (r) {
      case Role::Parent:        Parent::setup(); break;
      case Role::Child:         Child::setup();  break;
      case Role::Unprovisioned: Child::setupUnprovisioned(); break;
    }
  }

  void connected() {
    switch (getRole()) {
      case Role::Parent:        Parent::connected(); break;
      case Role::Child:         Child::connected();  break;
      case Role::Unprovisioned: break;
    }
  }

  void loop() {
    switch (getRole()) {
      case Role::Parent:        Parent::loop(); break;
      case Role::Child:         Child::loop();  break;
      case Role::Unprovisioned: Child::loopUnprovisioned(); break;
    }
    handleLoomxActions();
  }

}
