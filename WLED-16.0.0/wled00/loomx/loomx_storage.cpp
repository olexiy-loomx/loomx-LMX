#include "loomx_storage.h"
#include <Preferences.h>

// TODO(loomx-storage): use the same WLED_FS / Preferences split described in
// the header. Reuse writeObjectToFileUsingId from file.cpp for the per-MAC
// child config map.

namespace Loomx { namespace Storage {

  static constexpr const char* NVS_NS_PARENT = "loomx_parent";
  static constexpr const char* NVS_KEY_PARENT_SSID = "ssid";
  static constexpr const char* NVS_KEY_PARENT_PSK = "psk";
  static constexpr const char* NVS_NS_CONTROLLER = "loomx_child";
  static constexpr const char* NVS_KEY_CONTROLLER = "ctrl";
  static constexpr const char* NVS_NS_TRANSPORT = "loomx_transport";
  static constexpr const char* NVS_KEY_TRANSPORT = "type";

  bool init() { return true; }

  bool readParentCreds(String& outSSID, String& outPSK) {
    Preferences p;
    if (!p.begin(NVS_NS_PARENT, true)) return false;
    outSSID = p.getString(NVS_KEY_PARENT_SSID, "");
    outPSK = p.getString(NVS_KEY_PARENT_PSK, "");
    p.end();
    return outSSID.length() > 0;
  }

  bool writeParentCreds(const String& ssid, const String& psk) {
    Preferences p;
    if (!p.begin(NVS_NS_PARENT, false)) return false;
    const bool ok = p.putString(NVS_KEY_PARENT_SSID, ssid) > 0 &&
                    p.putString(NVS_KEY_PARENT_PSK, psk) > 0;
    p.end();
    return ok;
  }

  bool clearParentCreds() {
    Preferences p;
    if (!p.begin(NVS_NS_PARENT, false)) return false;
    const bool ok = p.remove(NVS_KEY_PARENT_SSID) && p.remove(NVS_KEY_PARENT_PSK);
    p.end();
    return ok;
  }

  ControllerType readControllerType() {
    Preferences p;
    if (!p.begin(NVS_NS_CONTROLLER, true)) return ControllerType::VX4;
    const uint8_t raw = p.getUChar(NVS_KEY_CONTROLLER, static_cast<uint8_t>(ControllerType::VX4));
    p.end();
    return raw == static_cast<uint8_t>(ControllerType::VX8) ? ControllerType::VX8 : ControllerType::VX4;
  }

  bool writeControllerType(ControllerType type) {
    Preferences p;
    if (!p.begin(NVS_NS_CONTROLLER, false)) return false;
    const bool ok = p.putUChar(NVS_KEY_CONTROLLER, static_cast<uint8_t>(type)) > 0;
    p.end();
    return ok;
  }

  const char* controllerTypeName(ControllerType type) {
    return type == ControllerType::VX8 ? "VX8" : "VX4";
  }

  ControllerType controllerTypeFromName(const String& name) {
    return name.equalsIgnoreCase(F("VX8")) ? ControllerType::VX8 : ControllerType::VX4;
  }

  TransportType readTransportType() {
    Preferences p;
    if (!p.begin(NVS_NS_TRANSPORT, true)) return TransportType::Http;
    const uint8_t raw = p.getUChar(NVS_KEY_TRANSPORT, static_cast<uint8_t>(TransportType::Http));
    p.end();
    switch (static_cast<TransportType>(raw)) {
      case TransportType::Udp:
      case TransportType::Tcp:
      case TransportType::WebSocket:
      case TransportType::Mqtt:
        return static_cast<TransportType>(raw);
      case TransportType::Http:
      default:
        return TransportType::Http;
    }
  }

  bool writeTransportType(TransportType type) {
    Preferences p;
    if (!p.begin(NVS_NS_TRANSPORT, false)) return false;
    const bool ok = p.putUChar(NVS_KEY_TRANSPORT, static_cast<uint8_t>(type)) > 0;
    p.end();
    return ok;
  }

  const char* transportTypeName(TransportType type) {
    switch (type) {
      case TransportType::Udp: return "udp";
      case TransportType::Tcp: return "tcp";
      case TransportType::WebSocket: return "websocket";
      case TransportType::Mqtt: return "mqtt";
      case TransportType::Http:
      default: return "http";
    }
  }

  TransportType transportTypeFromName(const String& name) {
    if (name.equalsIgnoreCase(F("udp"))) return TransportType::Udp;
    if (name.equalsIgnoreCase(F("tcp"))) return TransportType::Tcp;
    if (name.equalsIgnoreCase(F("websocket")) || name.equalsIgnoreCase(F("ws"))) return TransportType::WebSocket;
    if (name.equalsIgnoreCase(F("mqtt"))) return TransportType::Mqtt;
    return TransportType::Http;
  }

}}
