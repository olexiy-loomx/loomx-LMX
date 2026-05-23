#include "loomx_sequence.h"

// TODO(loomx-sequence): sequences are loaded on demand from
// /loomx_sequences/<name>.json via loomx_storage. A running sequence holds a
// step index, a step-end-at (parent millis) timestamp, and a loop flag. Each
// step broadcasts a SetState or PresetApply message per child with
// exec_at = now + LOOMX_LOCKSTEP_MARGIN_MS.

namespace Loomx { namespace Sequence {

  void tick() {}
  bool trigger(const char*) { return false; }
  void stop() {}

}}
