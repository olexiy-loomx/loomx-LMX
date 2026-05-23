#pragma once

#include <stdint.h>

namespace Loomx { namespace OutputMap {

  // Compile-time mapping of user-facing "Output N" indices to GPIO numbers.
  // Selected by the -D LOOMX_BOARD_* flag in platformio_override.ini. The
  // user never sees these GPIO numbers — they configure outputs by slot.
  struct Map {
    uint8_t       slotCount;
    const int8_t* gpioPerSlot;
  };

  const Map& current();

}}
