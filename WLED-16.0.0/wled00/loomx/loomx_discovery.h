#pragma once

#include <Arduino.h>
#include <vector>

namespace Loomx { namespace Discovery {

  struct ChildCandidate {
    String   ssid;
    int      rssi;
    uint8_t  bssid[6];
  };

  // Parent-side: scan for Loomx_Leaf_* SSIDs. Synchronous from the caller's
  // perspective; internally may yield while the WiFi scan completes.
  bool scan(std::vector<ChildCandidate>& outCandidates);

  // Parent-side: broadcast Parent creds to the candidate over ESP-NOW. Does
  // not affect the Parent's AP — UI stays connected. Returns true once the
  // Child acknowledges (also over ESP-NOW) and switches role.
  bool bind(const ChildCandidate& c, const char* parentSSID, const char* parentPSK);

}}
