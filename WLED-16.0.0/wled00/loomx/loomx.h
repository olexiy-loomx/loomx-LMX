#pragma once

// Loomx public entry points, called from the WLED v1 usermod hooks in
// wled00/usermod.cpp. Keep this header free of heavy includes so it can be
// pulled in by usermod.cpp without dragging the rest of Loomx in.

namespace Loomx {
  void setup();      // called once at boot, after FS mount, before WiFi
  void connected();  // called when network interfaces come up
  void loop();       // called every main loop iteration
}
