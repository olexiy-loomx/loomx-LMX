#include "loomx_ws_protocol.h"

// TODO(loomx-ws): reuse the AsyncWebSocket pattern from wled00/ws.cpp. Parent
// owns an AsyncWebSocket("/loomx-ws") attached to the existing WLED server,
// indexes connections by child MAC. Child uses AsyncWebSocketClient with
// aggressive reconnect (exponential backoff capped at ~5s). All messages
// carry msg_id; lockstep messages carry exec_at (parent millis() + margin).
// Children buffer exec_at messages and fire from clientLoop() once
// time_sync.localFor(exec_at) <= millis().

namespace Loomx { namespace WS {

  void serverInit() {}
  void serverLoop() {}
  void clientInit(const char*) {}
  void clientLoop() {}

}}
