#pragma once

namespace Loomx { namespace Child {

  // Provisioned (bound to a Parent): connects to Parent AP, runs WS client.
  void setup();
  void connected();
  void loop();

  // Unprovisioned (no Parent yet): runs its own Loomx_Leaf_NNNNN AP and waits
  // to be bound via the provisioning endpoint.
  void setupUnprovisioned();
  void loopUnprovisioned();

}}
