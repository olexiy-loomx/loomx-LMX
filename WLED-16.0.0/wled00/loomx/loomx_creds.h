#pragma once

#include <Arduino.h>

namespace Loomx { namespace Creds {

  // Shared password used by both Parent and unprovisioned Child APs.
  constexpr const char* LOOMX_AP_PASS = "loomxleaf";
  constexpr const char* CHILD_AP_PASS = LOOMX_AP_PASS;
  constexpr const char* PARENT_AP_PASS = LOOMX_AP_PASS;

  // Returns the device's stable 5-digit identity number (range 10000-99999).
  // Generated from esp_random() on first boot, persisted to NVS thereafter.
  // The same number is used in both the Parent SSID and the Child SSID
  // depending on the current role.
  uint32_t getDeviceNumber();

  // SSID & password generators. Output buffers must be large enough to hold
  // the result. All functions guarantee null-termination on success.
  //
  //   parent SSID format:    Loomx_Parent_NNNNN       (19 chars + null)
  //   parent password:       loomxleaf
  //   child  SSID format:    Loomx_Leaf_NNNNN         (16 chars + null)
  //   child  display name:   "Loomx Leaf NNNNN"       (16 chars + null)
  //
  // Parent password is also persisted in NVS to replace any older generated
  // password stored on already-flashed devices.
  bool generateParentSSID(char* out, size_t outLen);
  bool generateParentPassword(char* out, size_t outLen);
  bool generateChildSSID(char* out, size_t outLen);
  bool generateChildDisplayName(char* out, size_t outLen);

}}
