#include "loomx_child.h"
#include "loomx_creds.h"
#include "loomx_storage.h"
#include "loomx_time_sync.h"
#include "../wled.h"

// TODO(loomx-child): provisioned path opens a persistent WS connection to the
// stored Parent SSID and runs the WS client message handler
// (loomx_ws_protocol). The Unprovisioned path (implemented below for the AP
// credential override) will also need an ESP-NOW receive callback that
// listens for the Parent's signed credential broadcast and, on receipt,
// persists the creds, switches role to Child, and reboots.

namespace Loomx { namespace Child {

  static constexpr uint32_t RECONNECT_FORCE_INTERVAL_MS = 6000;
  static constexpr uint32_t PARENT_RESCAN_INTERVAL_MS = 30000;
  static uint32_t lastReconnectForceMs = 0;
  static uint32_t lastParentRescanMs = 0;
  static bool wasChildConnected = false;

  static bool findParentAP(String& outSSID) {
    int bestRssi = -128;
    String bestSSID;
    const int count = WiFi.scanNetworks(false, true);
    for (int i = 0; i < count; i++) {
      const String ssid = WiFi.SSID(i);
      if (!ssid.startsWith(F("Loomx_Parent_"))) continue;
      const int rssi = WiFi.RSSI(i);
      if (rssi > bestRssi) {
        bestRssi = rssi;
        bestSSID = ssid;
      }
    }
    WiFi.scanDelete();
    if (bestSSID.length() == 0) return false;
    outSSID = bestSSID;
    return true;
  }

  static void configureFallbackAP() {
    char ssid[20];
    if (!Creds::generateChildSSID(ssid, sizeof(ssid))) {
      DEBUG_PRINTLN(F("[Loomx::Child] cred generation failed"));
      return;
    }
    strlcpy(apSSID, ssid, sizeof(apSSID));
    strlcpy(apPass, Creds::CHILD_AP_PASS, sizeof(apPass));
  }

  void setup() {
    configureFallbackAP();
    apBehavior = AP_BEHAVIOR_NO_CONN;

    String parentSSID;
    String parentPSK;
    if (!Storage::readParentCreds(parentSSID, parentPSK) || parentSSID.length() == 0) {
      if (findParentAP(parentSSID)) {
        Storage::writeParentCreds(parentSSID, Creds::PARENT_AP_PASS);
      }
    }

    if (parentSSID.length() > 0) {
      parentPSK = Creds::PARENT_AP_PASS;
      Storage::writeParentCreds(parentSSID, parentPSK);
      if (multiWiFi.empty()) multiWiFi.emplace_back();
      strlcpy(multiWiFi[0].clientSSID, parentSSID.c_str(), sizeof(multiWiFi[0].clientSSID));
      strlcpy(multiWiFi[0].clientPass, parentPSK.length() ? parentPSK.c_str() : Creds::PARENT_AP_PASS, sizeof(multiWiFi[0].clientPass));
      selectedWiFi = 0;
      DEBUG_PRINTF_P(PSTR("[Loomx::Child] joining Parent AP SSID=%s\n"), multiWiFi[0].clientSSID);
      return;
    }

    DEBUG_PRINTLN(F("[Loomx::Child] no Parent AP found; fallback Leaf AP will open if disconnected"));
  }

  void connected() {
    TimeSync::reset();
    wasChildConnected = true;
    DEBUG_PRINTLN(F("[Loomx::Child] connected (stub)"));
  }

  void loop() {
    if (Network.isConnected()) {
      wasChildConnected = true;
      TimeSync::childPoll();
      return;
    }

    if (wasChildConnected) {
      TimeSync::reset();
      wasChildConnected = false;
    }

    const uint32_t now = millis();
    if (now - lastReconnectForceMs >= RECONNECT_FORCE_INTERVAL_MS) {
      forceReconnect = true;
      lastReconnectForceMs = now;
    }

    if (now - lastParentRescanMs >= PARENT_RESCAN_INTERVAL_MS) {
      String parentSSID;
      if (findParentAP(parentSSID) && parentSSID.length() > 0) {
        Storage::writeParentCreds(parentSSID, Creds::PARENT_AP_PASS);
        if (multiWiFi.empty()) multiWiFi.emplace_back();
        strlcpy(multiWiFi[0].clientSSID, parentSSID.c_str(), sizeof(multiWiFi[0].clientSSID));
        strlcpy(multiWiFi[0].clientPass, Creds::PARENT_AP_PASS, sizeof(multiWiFi[0].clientPass));
        selectedWiFi = 0;
        forceReconnect = true;
      }
      lastParentRescanMs = now;
    }
  }

  void setupUnprovisioned() {
    configureFallbackAP();
    apBehavior = AP_BEHAVIOR_ALWAYS;
    DEBUG_PRINTF_P(PSTR("[Loomx::Child] Unprovisioned AP SSID=%s\n"), apSSID);
  }

  void loopUnprovisioned() {
    // hot path — keep silent
  }

}}
