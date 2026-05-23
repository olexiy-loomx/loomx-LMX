#pragma once

#include <stdint.h>

namespace Loomx { namespace TimeSync {

  // Parent-side: broadcast a "time" message every ~500ms with current millis().
  void parentBroadcast();

  // Child-side: poll for parent time beacons.
  void childPoll();

  // Child-side: ingest a parent time value alongside arrival millis().
  void childIngest(uint32_t parentMillis, uint32_t childArrivalMillis);

  // Convert a parent-timeline millisecond timestamp to the local child timeline.
  // On the Parent this is a no-op (returns the input).
  uint32_t localFor(uint32_t parentMillis);

  // True once a child has received at least one valid parent time beacon.
  bool isSynced();

  // Clears child-side sync state after a disconnect so the next parent beacon
  // becomes the new reference point.
  void reset();

}}
