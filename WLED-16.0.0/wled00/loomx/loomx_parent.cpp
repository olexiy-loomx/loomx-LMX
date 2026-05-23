#include "loomx_parent.h"
#include "loomx_creds.h"
#include "loomx_time_sync.h"
#include "../wled.h"

// TODO(loomx-parent): once creds are in place, register the WS server
// endpoints (loomx_ws_protocol), boot the sequence engine (loomx_sequence),
// and own the child registry (loomx_storage). For now setup() handles only
// the AP credential override.

namespace Loomx { namespace Parent {

  void setup() {
    char ssid[20];
    char pass[24];
    if (!Creds::generateParentSSID(ssid, sizeof(ssid)) ||
        !Creds::generateParentPassword(pass, sizeof(pass))) {
      DEBUG_PRINTLN(F("[Loomx::Parent] cred generation failed"));
      return;
    }

    // apSSID / apPass / apBehavior are globals declared in wled.h. Overriding
    // them in userSetup() (which runs after cfg.json deserialisation but
    // before WiFi.begin) ensures WLED's initAP() uses our values.
    strlcpy(apSSID, ssid, sizeof(apSSID));
    strlcpy(apPass, pass, sizeof(apPass));
    apBehavior = AP_BEHAVIOR_ALWAYS; // AP stays up regardless of STA state

    DEBUG_PRINTF_P(PSTR("[Loomx::Parent] AP SSID=%s pass=%s\n"), apSSID, apPass);
  }

  void connected() {
    DEBUG_PRINTLN(F("[Loomx::Parent] connected (stub)"));
  }

  void loop() {
    TimeSync::parentBroadcast();
  }

}}
