#pragma once

#include <Arduino.h>

namespace Loomx { namespace Sequence {

  // Parent-only. Tick the engine: if a sequence is running and the current
  // step has elapsed, advance to the next step and broadcast the per-child
  // state via WS.
  void tick();

  // Start a sequence by name. Returns false if the sequence doesn't exist.
  bool trigger(const char* name);

  // Stop the currently running sequence (if any).
  void stop();

}}
