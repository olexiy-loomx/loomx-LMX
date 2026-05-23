#pragma once

#include <Arduino.h>

namespace Loomx { namespace WS {

  // Inbound message types, exchanged as JSON over the persistent
  // Parent<->Child WebSocket. Wire schema lives in docs/PROTOCOL.md (TBD).
  enum class MsgType : uint8_t {
    Unknown = 0,
    // Parent -> Child
    PresetApply,
    SetState,
    SequenceStart,
    ConfigPush,
    TimeSync,
    OtaPushBegin,
    // Child -> Parent
    Status,
    Ack,
    ProvisionRequest,
  };

  // Parent-side server hooks
  void serverInit();
  void serverLoop();

  // Child-side client hooks
  void clientInit(const char* parentSSID);
  void clientLoop();

}}
