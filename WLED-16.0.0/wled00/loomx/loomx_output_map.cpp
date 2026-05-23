#include "loomx_output_map.h"

namespace Loomx { namespace OutputMap {

  #if defined(LOOMX_BOARD_VX4) || defined(LOOMX_BOARD_4CH_GENERIC)
    static const int8_t kPins[] = { 16, 3, 1, 4 };
    static const Map kMap = { 4, kPins };
  #elif defined(LOOMX_BOARD_VX8) || defined(LOOMX_BOARD_8CH_GENERIC)
    static const int8_t kPins[] = { 0, 1, 2, 3, 4, 5, 12, 13 };
    static const Map kMap = { 8, kPins };
  #else
    // LOOMX_BOARD_NONE — Parent / dev board with no LED outputs wired.
    static const int8_t kPins[] = { -1 };
    static const Map kMap = { 0, kPins };
  #endif

  const Map& current() { return kMap; }

}}
