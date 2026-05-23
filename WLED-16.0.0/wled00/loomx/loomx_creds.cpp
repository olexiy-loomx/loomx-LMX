#include "loomx_creds.h"
#include <Preferences.h>
#include <esp_random.h>

namespace Loomx { namespace Creds {

  static constexpr const char* NVS_NS             = "loomx";
  static constexpr const char* NVS_KEY_DEV_NUM    = "dev_num";
  static constexpr const char* NVS_KEY_PARENT_PSK = "parent_pw";

  uint32_t getDeviceNumber() {
    Preferences p;
    if (p.begin(NVS_NS, true /*ro*/)) {
      uint32_t n = p.getUInt(NVS_KEY_DEV_NUM, 0);
      p.end();
      if (n >= 10000 && n <= 99999) return n;
    }
    uint32_t fresh = (esp_random() % 90000) + 10000;
    Preferences pw;
    if (pw.begin(NVS_NS, false /*rw*/)) {
      pw.putUInt(NVS_KEY_DEV_NUM, fresh);
      pw.end();
    }
    return fresh;
  }

  bool generateParentSSID(char* out, size_t outLen) {
    if (!out || outLen < 20) return false;
    snprintf(out, outLen, "Loomx_Parent_%05u", (unsigned)getDeviceNumber());
    return true;
  }

  bool generateChildSSID(char* out, size_t outLen) {
    if (!out || outLen < 17) return false;
    snprintf(out, outLen, "Loomx_Leaf_%05u", (unsigned)getDeviceNumber());
    return true;
  }

  bool generateChildDisplayName(char* out, size_t outLen) {
    if (!out || outLen < 17) return false;
    snprintf(out, outLen, "Loomx Leaf %05u", (unsigned)getDeviceNumber());
    return true;
  }

  bool generateParentPassword(char* out, size_t outLen) {
    if (!out || outLen <= strlen(PARENT_AP_PASS)) return false;
    strlcpy(out, PARENT_AP_PASS, outLen);

    // Replace the old generated Parent password on already-flashed devices.
    Preferences pw;
    if (pw.begin(NVS_NS, false /*rw*/)) {
      pw.putString(NVS_KEY_PARENT_PSK, PARENT_AP_PASS);
      pw.end();
    }
    return true;
  }

}}
