#pragma once

#include <Arduino.h>

namespace Loomx { namespace Storage {

  // Small key-value (role, parent credentials, last-bound parent SSID) lives
  // in NVS via the Preferences API — see loomx_role.cpp for the pattern.
  //
  // JSON blobs (child configs keyed by MAC, presets, sequences) live in
  // LittleFS under /loomx/. WLED's WLED_FS handle is the same filesystem.
  //
  // Per the spec: on child reconnect, the Parent does NOT auto-push if it
  // already has a stored config for that MAC. Only new MACs get the generic
  // default. This rule is enforced in loomx_parent on the WS_EVT_CONNECT path.

  bool init();    // creates /loomx/ if missing

  // Parent credential persistence on the Child (which Parent SSID to join).
  bool readParentCreds (String& outSSID, String& outPSK);
  bool writeParentCreds(const String& ssid, const String& psk);
  bool clearParentCreds();

  enum class ControllerType : uint8_t {
    VX4 = 4,
    VX8 = 8,
  };

  ControllerType readControllerType();
  bool writeControllerType(ControllerType type);
  const char* controllerTypeName(ControllerType type);
  ControllerType controllerTypeFromName(const String& name);

  enum class TransportType : uint8_t {
    Http = 0,
    Udp,
    Tcp,
    WebSocket,
    Mqtt,
  };

  TransportType readTransportType();
  bool writeTransportType(TransportType type);
  const char* transportTypeName(TransportType type);
  TransportType transportTypeFromName(const String& name);

}}
