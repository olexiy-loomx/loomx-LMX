#include "loomx_discovery.h"
#include "../wled.h"

// TODO(loomx-discovery): scan() reuses the WLED async WiFi scan pattern from
// network.cpp::findWiFi, filtering SSIDs by the "Loomx_Leaf_" prefix.
//
// bind() does NOT switch the Parent's AP. Instead it broadcasts a small
// signed ESP-NOW frame (the Parent's SSID + PSK, plus the target Child's MAC)
// on the channel the Children are listening on while Unprovisioned. Children
// pick up the frame, persist the creds, switch to Child role, and connect to
// the Parent's AP. The operator's UI stays connected to the Parent throughout
// — they just see "Searching... N found... [Add]".

namespace Loomx { namespace Discovery {

  bool scan(std::vector<ChildCandidate>& outCandidates) {
    outCandidates.clear();
    const int count = WiFi.scanNetworks(false, true);
    if (count < 0) return false;

    for (int i = 0; i < count; i++) {
      const String ssid = WiFi.SSID(i);
      if (!ssid.startsWith(F("Loomx_Leaf_"))) continue;
      if (ssid == String(apSSID)) continue; // never list this controller's own AP

      ChildCandidate candidate;
      candidate.ssid = ssid;
      candidate.rssi = WiFi.RSSI(i);
      const uint8_t* bssid = WiFi.BSSID(i);
      if (bssid) memcpy(candidate.bssid, bssid, sizeof(candidate.bssid));
      else memset(candidate.bssid, 0, sizeof(candidate.bssid));
      outCandidates.push_back(candidate);
    }

    WiFi.scanDelete();
    return true;
  }

  bool bind(const ChildCandidate&, const char*, const char*) { return false; }

}}
