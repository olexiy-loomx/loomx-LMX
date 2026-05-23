#include "wled.h"
#include "loomx/loomx.h"
/*
 * Loomx forks the v1 usermod hooks. The original empty stubs forward into
 * the Loomx boot dispatcher (loomx.cpp), which routes to Parent/Child/
 * Unprovisioned setup paths based on the NVS-persisted role.
 */

void userSetup()     { Loomx::setup();     }
void userConnected() { Loomx::connected(); }
void userLoop()      { Loomx::loop();      }
