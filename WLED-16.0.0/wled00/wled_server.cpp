#include "wled.h"

#ifndef WLED_DISABLE_OTA
  #include "ota_update.h"  
#endif
#include "html_ui.h"
#include "html_settings.h"
#include "html_other.h"
#include "js_iro.h"
#include "js_omggif.h"
#ifdef WLED_ENABLE_PIXART
  #include "html_pixart.h"
#endif
#ifdef WLED_ENABLE_PXMAGIC
  #include "html_pxmagic.h"
#endif
#ifndef WLED_DISABLE_PIXELFORGE
  #include "html_pixelforge.h"
#endif
#include "html_cpal.h"
#include "html_edit.h"
#include "loomx/loomx.h"
#include "loomx/loomx_creds.h"
#include "loomx/loomx_discovery.h"
#include "loomx/loomx_role.h"
#include "loomx/loomx_storage.h"
#include "loomx/loomx_time_sync.h"
#ifdef ARDUINO_ARCH_ESP32
  #include <HTTPClient.h>
  #include <WiFiClient.h>
  #include <WiFiUdp.h>
  #include <esp_app_format.h>
  #include <esp_ota_ops.h>
  #include <esp_wifi.h>
  #include <tcpip_adapter.h>
#endif

// forward declarations
static void createEditHandler();
static void createLoomxApiHandlers();


// define flash strings once (saves flash memory)
static const char s_redirecting[] PROGMEM = "Redirecting...";
static const char s_content_enc[] PROGMEM = "Content-Encoding";
static const char s_unlock_ota [] PROGMEM = "Please unlock OTA in security settings!";
static const char s_unlock_cfg [] PROGMEM = "Please unlock settings using PIN code!";
static const char s_rebooting  [] PROGMEM = "Rebooting now...";
static const char s_notimplemented[] PROGMEM = "Not implemented";
static const char s_accessdenied[]   PROGMEM = "Access Denied";
static const char s_not_found[]      PROGMEM = "Not found";
static const char s_wsec[]           PROGMEM = "wsec.json";
static const char s_func[]           PROGMEM = "func";
static const char s_list[]           PROGMEM = "list";
static const char s_path[]           PROGMEM = "path";
static const char s_cache_control[]  PROGMEM = "Cache-Control";
static const char s_no_store[]       PROGMEM = "no-store";
static const char s_expires[]        PROGMEM = "Expires";
static const char _common_js[]       PROGMEM = "/common.js";
static const char _iro_js[]          PROGMEM = "/iro.js";
static const char _omggif_js[]       PROGMEM = "/omggif.js";
static constexpr size_t LOOMX_ACTION_JSON_CAPACITY = 6144;
static constexpr uint8_t LOOMX_ACTION_QUEUE_MAX = 8;
static constexpr uint16_t LOOMX_ACTION_UDP_PORT = 42717;
static constexpr uint16_t LOOMX_ACTION_TCP_PORT = 42718;
static constexpr uint16_t LOOMX_ACTION_MQTT_PORT = 1883;

//Is this an IP?
static bool isIp(const String &str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

static bool inSubnet(const IPAddress &ip, const IPAddress &subnet, const IPAddress &mask) {
  return (((uint32_t)ip & (uint32_t)mask) == ((uint32_t)subnet & (uint32_t)mask));
}

static bool inSameSubnet(const IPAddress &client) {
  return inSubnet(client, Network.localIP(), Network.subnetMask());
}

static bool inLocalSubnet(const IPAddress &client) {
  return  inSubnet(client, IPAddress(10,0,0,0),    IPAddress(255,0,0,0))                  // 10.x.x.x
      ||  inSubnet(client, IPAddress(192,168,0,0), IPAddress(255,255,0,0))                // 192.168.x.x
      ||  inSubnet(client, IPAddress(172,16,0,0),  IPAddress(255,240,0,0))                // 172.16.x.x
      || (inSubnet(client, IPAddress(4,3,2,0),     IPAddress(255,255,255,0)) && apActive) // WLED AP
      ||  inSameSubnet(client);                                                           // same subnet as WLED device
}

/*
 * Integrated HTTP web server page declarations
 */

static void generateEtag(char *etag, uint16_t eTagSuffix) {
  sprintf_P(etag, PSTR("%u-%02x-%04x"), WEB_BUILD_TIME, cacheInvalidate, eTagSuffix);
}

static void setStaticContentCacheHeaders(AsyncWebServerResponse *response, int code, uint16_t eTagSuffix = 0) {
  // Only send ETag for 200 (OK) responses
  if (code != 200) return;

  // https://medium.com/@codebyamir/a-web-developers-guide-to-browser-caching-cc41f3b73e7c
  #ifndef WLED_DEBUG
  // this header name is misleading, "no-cache" will not disable cache,
  // it just revalidates on every load using the "If-None-Match" header with the last ETag value
  response->addHeader(FPSTR(s_cache_control), F("no-cache"));
  #else
  response->addHeader(FPSTR(s_cache_control), F("no-store,max-age=0"));  // prevent caching if debug build
  #endif
  char etag[32];
  generateEtag(etag, eTagSuffix);
  response->addHeader(F("ETag"), etag);
}

static bool handleIfNoneMatchCacheHeader(AsyncWebServerRequest *request, int code, uint16_t eTagSuffix = 0) {
  // Only send 304 (Not Modified) if response code is 200 (OK)
  if (code != 200) return false;

  AsyncWebHeader *header = request->getHeader(F("If-None-Match"));
  char etag[32];
  generateEtag(etag, eTagSuffix);
  if (header && header->value() == etag) {
    AsyncWebServerResponse *response = request->beginResponse(304);
    setStaticContentCacheHeaders(response, code, eTagSuffix);
    request->send(response);
    return true;
  }
  return false;
}

/**
 * Handles the request for a static file.
 * If the file was found in the filesystem, it will be sent to the client.
 * Otherwise it will be checked if the browser cached the file and if so, a 304 response will be sent.
 * If the file was not found in the filesystem and not in the browser cache, the request will be handled as a 200 response with the content of the page.
 *
 * @param request The request object
 * @param path If a file with this path exists in the filesystem, it will be sent to the client. Set to "" to skip this check.
 * @param code The HTTP status code
 * @param contentType The content type of the web page
 * @param content Content of the web page
 * @param len Length of the content
 * @param gzip Optional. Defaults to true. If false, the gzip header will not be added.
 * @param eTagSuffix Optional. Defaults to 0. A suffix that will be added to the ETag header. This can be used to invalidate the cache for a specific page.
 */
static void handleStaticContent(AsyncWebServerRequest *request, const String &path, int code, const String &contentType, const uint8_t *content, size_t len, bool gzip = true, uint16_t eTagSuffix = 0) {
  if (path != "" && handleFileRead(request, path)) return;
  if (handleIfNoneMatchCacheHeader(request, code, eTagSuffix)) return;
  AsyncWebServerResponse *response = request->beginResponse_P(code, contentType, content, len);
  if (gzip) response->addHeader(FPSTR(s_content_enc), F("gzip"));
  setStaticContentCacheHeaders(response, code, eTagSuffix);
  request->send(response);
}

static void serveLoomxIndex(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, FPSTR(CONTENT_TYPE_HTML), PAGE_index, PAGE_index_length);
  response->addHeader(FPSTR(s_content_enc), F("gzip"));
  response->addHeader(FPSTR(s_cache_control), F("no-store,max-age=0"));
  request->send(response);
}

#ifdef WLED_ENABLE_DMX
static String dmxProcessor(const String& var)
{
  String mapJS;
  if (var == F("DMXVARS")) {
    mapJS += F("\nCN=");
    mapJS += String(DMXChannels);
    mapJS += F(";\nCS=");
    mapJS += String(DMXStart);
    mapJS += F(";\nCG=");
    mapJS += String(DMXGap);
    mapJS += F(";\nLC=");
    mapJS += String(strip.getLengthTotal());
    mapJS += F(";\nvar CH=[");
    for (int i=0; i<15; i++) {
      mapJS += String(DMXFixtureMap[i]) + ',';
    }
    mapJS += F("0];");
  }
  return mapJS;
}
#endif

static String msgProcessor(const String& var)
{
  if (var == "MSG") {
    String messageBody = messageHead;
    messageBody += F("</h2>");
    messageBody += messageSub;
    uint32_t optt = optionType;

    if (optt < 60) //redirect to settings after optionType seconds
    {
      messageBody += F("<script>setTimeout(RS,");
      messageBody += String(optt*1000);
      messageBody += F(")</script>");
    } else if (optt < 120) //redirect back after optionType-60 seconds, unused
    {
      //messageBody += "<script>setTimeout(B," + String((optt-60)*1000) + ")</script>";
    } else if (optt < 180) //reload parent after optionType-120 seconds
    {
      messageBody += F("<script>setTimeout(RP,");
      messageBody += String((optt-120)*1000);
      messageBody += F(")</script>");
    } else if (optt == 253)
    {
      messageBody += F("<br><br><form action=/settings><button class=\"bt\" type=submit>Back</button></form>"); //button to settings
    } else if (optt == 254)
    {
      messageBody += F("<br><br><button type=\"button\" class=\"bt\" onclick=\"B()\">Back</button>");
    }
    return messageBody;
  }
  return String();
}


static void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool isFinal) {
  if (!correctPIN) {
    if (isFinal) request->send(401, FPSTR(CONTENT_TYPE_PLAIN), FPSTR(s_unlock_cfg));
    return;
  }
  if (!index) {
    String finalname = filename;
    if (finalname.charAt(0) != '/') {
      finalname = '/' + finalname; // prepend slash if missing
    }

    request->_tempFile = WLED_FS.open(finalname, "w");
    DEBUG_PRINTF_P(PSTR("Uploading %s\n"), finalname.c_str());
    if (finalname.equals(FPSTR(getPresetsFileName()))) presetsModifiedTime = toki.second();
  }
  if (len) {
    request->_tempFile.write(data,len);
  }
  if (isFinal) {
    request->_tempFile.close();
    if (filename.indexOf(F("cfg.json")) >= 0) { // check for filename with or without slash
      doReboot = true;
      request->send(200, FPSTR(CONTENT_TYPE_PLAIN), F("Config restore ok.\nRebooting..."));
    } else {
      if (filename.indexOf(F("palette")) >= 0 && filename.indexOf(F(".json")) >= 0) loadCustomPalettes();
      request->send(200, FPSTR(CONTENT_TYPE_PLAIN), F("File Uploaded!"));
    }
    cacheInvalidate++;
    updateFSInfo(); // refresh memory usage info
  }
}

static const char _edit_htm[] PROGMEM = "/edit.htm";

static void createEditHandler() {
  if (editHandler != nullptr) server.removeHandler(editHandler);

  editHandler = &server.on(F("/edit"), static_cast<WebRequestMethod>(HTTP_GET), [](AsyncWebServerRequest *request) {
    // PIN check for GET/DELETE, for POST it is done in handleUpload()
    if (!correctPIN) {
      serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_cfg), 254);
      return;
    }
    const String& func = request->arg(FPSTR(s_func));
    bool legacyList = false;
    if (request->hasArg(FPSTR(s_list))) {
      legacyList = true; // support for '?list=/'
    }

    if(func.length() == 0 && !legacyList) {
      // default: serve the editor page
      handleStaticContent(request, FPSTR(_edit_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_edit, PAGE_edit_length);
      return;
    }

    if (func == FPSTR(s_list) || legacyList) {
      bool first = true;
      AsyncResponseStream* response = request->beginResponseStream(FPSTR(CONTENT_TYPE_JSON));
      response->addHeader(FPSTR(s_cache_control), FPSTR(s_no_store));
      response->addHeader(FPSTR(s_expires), F("0"));
      response->write('[');

      File rootdir = WLED_FS.open("/", "r");
      File rootfile = rootdir.openNextFile();
      while (rootfile) {
        String name = rootfile.name();
        if (name.indexOf(FPSTR(s_wsec)) >= 0) {
          rootfile = rootdir.openNextFile(); // skip wsec.json
          continue;
        }
        if (!first) response->write(',');
        first = false;
        response->printf_P(PSTR("{\"name\":\"%s\",\"type\":\"file\",\"size\":%u}"), name.c_str(), rootfile.size());
        rootfile = rootdir.openNextFile();
      }
      rootfile.close();
      rootdir.close();
      response->write(']');
      request->send(response);
      return;
    }

    String path = request->arg(FPSTR(s_path)); // remaining functions expect a path

    if (path.length() == 0) {
      request->send(400, FPSTR(CONTENT_TYPE_PLAIN), F("Missing path"));
      return;
    }

    if (path.charAt(0) != '/') {
      path = '/' + path; // prepend slash if missing
    }

    if (!WLED_FS.exists(path)) {
      request->send(404, FPSTR(CONTENT_TYPE_PLAIN), FPSTR(s_not_found));
      return;
    }

    if (path.indexOf(FPSTR(s_wsec)) >= 0) {
      request->send(403, FPSTR(CONTENT_TYPE_PLAIN), FPSTR(s_accessdenied)); // skip wsec.json
      return;
    }

    if (func == "edit") {
      request->send(WLED_FS, path);
      return;
    }

    if (func == "download") {
      request->send(WLED_FS, path, String(), true);
      return;
    }

    if (func == "delete") {
      if (!WLED_FS.remove(path))
        request->send(500, FPSTR(CONTENT_TYPE_PLAIN), F("Delete failed"));
      else
        request->send(200, FPSTR(CONTENT_TYPE_PLAIN), F("File deleted"));
      updateFSInfo(); // refresh memory usage info
      return;
    }

    // unrecognized func
    request->send(400, FPSTR(CONTENT_TYPE_PLAIN), F("Invalid function"));
  });
}

static bool captivePortal(AsyncWebServerRequest *request)
{
  if (!apActive) return false; //only serve captive in AP mode
  if (!request->hasHeader(F("Host"))) return false;

  String hostH = request->getHeader(F("Host"))->value();
  if (!isIp(hostH) && hostH.indexOf(F("wled.me")) < 0 && hostH.indexOf(cmDNS) < 0 && hostH.indexOf(':') < 0) {
    DEBUG_PRINTLN(F("Captive portal"));
    AsyncWebServerResponse *response = request->beginResponse(302);
    response->addHeader(F("Location"), F("http://4.3.2.1"));
    request->send(response);
    return true;
  }
  return false;
}

static void sendLoomxJson(AsyncWebServerRequest *request, const String& payload, uint16_t code = 200)
{
  request->send(code, FPSTR(CONTENT_TYPE_JSON), payload);
}

static void appendLoomxJsonString(String& out, const String& value)
{
  out += '"';
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += '"';
}

static String loomxMacToString(const uint8_t* mac)
{
  char out[18];
  snprintf_P(out, sizeof(out), PSTR("%02X:%02X:%02X:%02X:%02X:%02X"),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(out);
}

static bool loomxHexToRgb(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b)
{
  if (!hex) return false;
  if (hex[0] == '#') hex++;
  if (strlen(hex) < 6) return false;
  char part[3] = {0, 0, 0};
  part[0] = hex[0]; part[1] = hex[1]; r = strtoul(part, nullptr, 16);
  part[0] = hex[2]; part[1] = hex[3]; g = strtoul(part, nullptr, 16);
  part[0] = hex[4]; part[1] = hex[5]; b = strtoul(part, nullptr, 16);
  return true;
}

static uint8_t loomxEffectId(const String& effect)
{
  if (effect == F("flash")) return FX_MODE_BLINK;
  if (effect == F("fade")) return FX_MODE_FADE;
  if (effect == F("pulse")) return FX_MODE_BREATH;
  if (effect == F("wipe")) return FX_MODE_COLOR_WIPE;
  if (effect == F("theater")) return FX_MODE_THEATER_CHASE;
  if (effect == F("twinkle")) return FX_MODE_TWINKLE;
  if (effect == F("sparkle")) return FX_MODE_SPARKLE;
  if (effect == F("strobe")) return FX_MODE_STROBE;
  if (effect == F("chase")) return FX_MODE_CHASE_COLOR;
  if (effect == F("chase_rainbow")) return FX_MODE_CHASE_RAINBOW;
  if (effect == F("rainbow")) return FX_MODE_RAINBOW_CYCLE;
  if (effect == F("scanner")) return FX_MODE_LARSON_SCANNER;
  if (effect == F("comet")) return FX_MODE_COMET;
  if (effect == F("meteor")) return FX_MODE_METEOR;
  if (effect == F("fire")) return FX_MODE_FIRE_2012;
  if (effect == F("colorwaves")) return FX_MODE_COLORWAVES;
  if (effect == F("juggle")) return FX_MODE_JUGGLE;
  if (effect == F("sinelon")) return FX_MODE_SINELON;
  if (effect == F("noise")) return FX_MODE_NOISE16_1;
  if (effect == F("glitter")) return FX_MODE_GLITTER;
  if (effect == F("popcorn")) return FX_MODE_POPCORN;
  if (effect == F("plasma")) return FX_MODE_PLASMA;
  return FX_MODE_STATIC;
}

static int8_t loomxControllerPin(const String& controllerType, uint8_t slot)
{
  static const int8_t vx4Pins[] = { 16, 3, 1, 4 };
  static const int8_t vx8Pins[] = { 0, 1, 2, 3, 4, 5, 12, 13 };
  if (slot == 0) return -1;
  if (controllerType == F("VX8")) return slot <= 8 ? vx8Pins[slot - 1] : -1;
  return slot <= 4 ? vx4Pins[slot - 1] : -1;
}

static uint8_t loomxLedTypeId(const String& ledType)
{
  if (ledType == F("SK6812")) return TYPE_SK6812_RGBW;
  if (ledType == F("TM1814")) return TYPE_TM1814;
  if (ledType == F("APA102")) return TYPE_APA102;
  if (ledType == F("WS2811")) return TYPE_WS2812_RGB;
  return TYPE_WS2812_RGB;
}

static uint8_t loomxColorOrderId(const String& colorOrder)
{
  if (colorOrder == F("RGB")) return COL_ORDER_RGB;
  if (colorOrder == F("BRG")) return COL_ORDER_BRG;
  if (colorOrder == F("RBG")) return COL_ORDER_RBG;
  if (colorOrder == F("BGR")) return COL_ORDER_BGR;
  if (colorOrder == F("GBR")) return COL_ORDER_GBR;
  return COL_ORDER_GRB;
}

static bool loomxJsonObjectHasEntries(JsonObject object)
{
  if (object.isNull()) return false;
  for (JsonPair kv : object) {
    (void)kv;
    return true;
  }
  return false;
}

static JsonObject loomxOutputTargetsForAction(JsonObject root)
{
  JsonObject outputTargets = root["output_targets"].as<JsonObject>();
  if (loomxJsonObjectHasEntries(outputTargets)) return outputTargets;

  outputTargets = root["preset"]["output_targets"].as<JsonObject>();
  if (loomxJsonObjectHasEntries(outputTargets)) return outputTargets;

  return JsonObject();
}

static bool applyLoomxChildConfig(JsonObject config)
{
  const String controllerType = config["controller_type"] | "VX4";
  if (!Loomx::Storage::writeControllerType(Loomx::Storage::controllerTypeFromName(controllerType))) return false;

  JsonArray outputs = config["outputs"].as<JsonArray>();
  if (outputs.isNull()) return true;

  busConfigs.clear();
  uint16_t start = 0;
  uint8_t configured = 0;
  const uint8_t maxSlots = controllerType == F("VX8") ? 8 : 4;
  for (JsonObject output : outputs) {
    const uint8_t slot = output["slot"] | uint8_t(configured + 1);
    if (slot == 0 || slot > maxSlots) continue;

    const uint16_t count = output["count"] | 0;
    if (count == 0) continue;

    const int8_t gpio = loomxControllerPin(controllerType, slot);
    if (gpio < 0) continue;

    String colorOrderName = output["color_order"] | "GRB";
    colorOrderName.toUpperCase();
    String ledTypeName = output["led_type"] | "WS2812B";
    ledTypeName.toUpperCase();

    uint8_t pins[OUTPUT_MAX_PINS] = {255, 255, 255, 255, 255};
    pins[0] = gpio;
    if (ledTypeName == F("APA102")) pins[1] = 255; // VX outputs expose one data pin per slot.

    busConfigs.emplace_back(
      loomxLedTypeId(ledTypeName),
      pins,
      start,
      count,
      loomxColorOrderId(colorOrderName),
      false,
      0,
      RGBW_MODE_MANUAL_ONLY,
      0,
      LED_MILLIAMPS_DEFAULT,
      0,
      0
    );
    start += count;
    configured++;
  }

  if (configured == 0) return false;
  BusManager::setMilliampsMax(0);
  doInitBusses = true;
  configNeedsWrite = true;
  return true;
}

static uint8_t loomxControlByte(JsonVariant value, uint8_t fallback = 128)
{
  if (value.isNull()) return fallback;
  return constrain(value.as<int>(), 0, 255);
}

static void appendLoomxSegment(JsonArray segs, uint8_t id, uint16_t start, uint16_t stop, uint8_t fx, uint8_t r, uint8_t g, uint8_t b, uint8_t speed, uint8_t frequency)
{
  JsonObject seg = segs.createNestedObject();
  seg["id"] = id;
  seg["start"] = start;
  seg["stop"] = stop;
  seg["on"] = true;
  seg["fx"] = fx;
  seg["sx"] = speed;
  seg["ix"] = frequency;
  JsonArray col = seg.createNestedArray("col");
  JsonArray rgb = col.createNestedArray();
  rgb.add(r);
  rgb.add(g);
  rgb.add(b);
}

static void appendLoomxDeletedSegment(JsonArray segs, uint8_t id)
{
  JsonObject seg = segs.createNestedObject();
  seg["id"] = id;
  seg["stop"] = 0;
}

static uint8_t appendLoomxOutputSegments(JsonArray segs, JsonArray targetOutputs, uint8_t fx, uint8_t r, uint8_t g, uint8_t b, uint8_t speed, uint8_t frequency)
{
  if (targetOutputs.isNull()) return 0;

  uint8_t used = 0;
  bool seen[9] = {};
  for (JsonVariant targetOutput : targetOutputs) {
    const int slot = targetOutput.as<int>();
    if (slot <= 0 || slot > 8 || seen[slot]) continue;
    seen[slot] = true;

    Bus* bus = BusManager::getBus(size_t(slot - 1));
    if (!bus || !bus->isOk()) continue;

    const uint16_t start = bus->getStart();
    const uint16_t stop = start + bus->getLength();
    if (stop <= start) continue;

    appendLoomxSegment(segs, used++, start, stop, fx, r, g, b, speed, frequency);
  }
  return used;
}

static void appendLoomxSegmentCleanup(JsonArray segs, uint8_t used)
{
  const size_t busCount = BusManager::getNumBusses();
  const uint8_t configured = busCount > 8 ? 8 : busCount;
  const uint8_t cleanupUntil = configured > used ? configured : used;
  for (uint8_t id = used; id < cleanupUntil; id++) appendLoomxDeletedSegment(segs, id);
}

static bool applyLoomxAction(JsonObject root)
{
  const String type = root["type"] | "";
  if (type == F("child_config")) {
    JsonObject config = root["config"].as<JsonObject>();
    return applyLoomxChildConfig(config);
  }

  if (type == F("master_power")) {
    DynamicJsonDocument state(128);
    const int brightness = root["brightness"] | 255;
    state["on"] = root["on"] | false;
    state["bri"] = constrain(brightness, 0, 255);
    return deserializeState(state.as<JsonObject>(), CALL_MODE_DIRECT_CHANGE);
  }

  if (type == F("master_brightness")) {
    DynamicJsonDocument state(128);
    const int brightness = root["brightness"] | 255;
    const uint16_t fadeMs = root["fade_ms"] | 800;
    if (!root["on"].isNull()) state["on"] = root["on"] | true;
    state["bri"] = constrain(brightness, 0, 255);
    state["transition"] = max<uint16_t>(1, fadeMs / 100);
    return deserializeState(state.as<JsonObject>(), CALL_MODE_DIRECT_CHANGE);
  }

  if (type == F("stop_all")) {
    DynamicJsonDocument state(128);
    state["on"] = false;
    return deserializeState(state.as<JsonObject>(), CALL_MODE_DIRECT_CHANGE);
  }

  JsonVariant colorVariant;
  String effect = F("solid");
  uint16_t fadeMs = 0;
  int brightness = root["brightness"] | 255;
  uint8_t speed = loomxControlByte(root["speed"], 128);
  uint8_t frequency = loomxControlByte(root["frequency"], 128);

  if (type == F("sequence_trigger")) {
    JsonArray steps = root["sequence"]["steps"].as<JsonArray>();
    if (!steps.isNull() && steps.size() > 0) {
      JsonObject firstStep = steps[0].as<JsonObject>();
      colorVariant = firstStep["color"];
      effect = firstStep["effect"] | "solid";
      fadeMs = firstStep["fade_ms"] | 0;
      brightness = firstStep["brightness"] | brightness;
      speed = loomxControlByte(firstStep["speed"], speed);
      frequency = loomxControlByte(firstStep["frequency"], frequency);
    }
  } else if (type == F("sequence_step") || type == F("preview_step")) {
    colorVariant = root["color"];
    effect = root["effect"] | "solid";
    fadeMs = root["fade_ms"] | 0;
    brightness = root["brightness"] | brightness;
    speed = loomxControlByte(root["speed"], speed);
    frequency = loomxControlByte(root["frequency"], frequency);
  } else if (type == F("preset_apply")) {
    JsonObject presetState = root["state"].as<JsonObject>();
    if (presetState.isNull()) presetState = root["preset"]["state"].as<JsonObject>();
    if (!presetState.isNull()) {
      colorVariant = presetState["color"];
      effect = presetState["effect"] | "solid";
      brightness = presetState["brightness"] | brightness;
      speed = loomxControlByte(presetState["speed"], speed);
      frequency = loomxControlByte(presetState["frequency"], frequency);
    } else {
      JsonObject states = root["preset"]["states"].as<JsonObject>();
      for (JsonPair kv : states) {
        colorVariant = kv.value()["color"];
        effect = kv.value()["effect"] | "solid";
        brightness = kv.value()["brightness"] | brightness;
        speed = loomxControlByte(kv.value()["speed"], speed);
        frequency = loomxControlByte(kv.value()["frequency"], frequency);
        break;
      }
      if (colorVariant.isNull()) {
        JsonArray colors = root["preset"]["colors"].as<JsonArray>();
        if (!colors.isNull() && colors.size() > 0) colorVariant = colors[0];
      }
    }
  } else if (type == F("identify")) {
    colorVariant.set("#ffffff");
    effect = F("flash");
  }

  uint8_t r = 255, g = 255, b = 255;
  const char* color = colorVariant | "#ffffff";
  loomxHexToRgb(color, r, g, b);

  DynamicJsonDocument state(LOOMX_ACTION_JSON_CAPACITY);
  state["on"] = true;
  state["bri"] = constrain(brightness, 0, 255);
  state["transition"] = fadeMs / 100;
  if (!root["sync_timebase"].isNull()) state["tb"] = root["sync_timebase"].as<uint32_t>();
  JsonArray segs = state.createNestedArray("seg");
  const uint8_t fx = loomxEffectId(effect);
  JsonArray targetOutputs = root["target_outputs"].as<JsonArray>();
  uint8_t usedSegments = appendLoomxOutputSegments(segs, targetOutputs, fx, r, g, b, speed, frequency);
  if (usedSegments == 0) {
    if (!targetOutputs.isNull()) return false;
    appendLoomxSegment(segs, 0, 0, strip.getLengthTotal(), fx, r, g, b, speed, frequency);
    usedSegments = 1;
  }
  appendLoomxSegmentCleanup(segs, usedSegments);
  return deserializeState(state.as<JsonObject>(), CALL_MODE_DIRECT_CHANGE);
}

static bool loomxActionShouldSync(const String& type)
{
  return type != F("child_config");
}

static constexpr uint32_t LOOMX_VISUAL_SYNC_LEAD_MS = 450;
static constexpr uint32_t LOOMX_FAST_SYNC_LEAD_MS = 150;
static constexpr uint32_t LOOMX_SYNC_MIN_DELAY_MS = 15;

static bool loomxActionUsesEffectTimebase(const String& type)
{
  return type == F("sequence_trigger") ||
         type == F("sequence_step") ||
         type == F("preview_step") ||
         type == F("preset_apply") ||
         type == F("identify");
}

static uint32_t loomxSyncLeadMsForType(const String& type)
{
  return loomxActionUsesEffectTimebase(type) ? LOOMX_VISUAL_SYNC_LEAD_MS : LOOMX_FAST_SYNC_LEAD_MS;
}

static bool loomxActionIsSequence(const String& type)
{
  return type == F("sequence_trigger") || type == F("sequence_step");
}

static bool loomxActionReplacesPendingVisual(const String& type)
{
  return type == F("preview_step") ||
         type == F("preset_apply") ||
         type == F("identify") ||
         type == F("stop_all") ||
         type == F("master_power") ||
         type == F("master_brightness");
}

static bool loomxActionCancelsSequence(JsonObject root, const String& type)
{
  if (root["cancel_sequence"] | false) return true;
  return type == F("preset_apply") || type == F("stop_all");
}

static volatile uint32_t loomxSequenceScheduleGeneration = 0;
static volatile uint32_t loomxVisualScheduleGeneration = 0;
static volatile uint32_t loomxForwardScheduleGeneration = 0;
static uint32_t loomxLatestVisualCommandMarker = 0;
static uint32_t loomxLatestVisualCommandReceivedMs = 0;
static constexpr uint32_t LOOMX_VISUAL_STALE_WINDOW_MS = 10000;

static void loomxCancelScheduledSequences()
{
  loomxSequenceScheduleGeneration++;
}

static uint32_t loomxRegisterVisualAction(const String& type)
{
  if (loomxActionReplacesPendingVisual(type)) loomxVisualScheduleGeneration++;
  return loomxVisualScheduleGeneration;
}

static bool loomxVisualActionIsStale(JsonObject root, const String& type)
{
  if (!loomxActionReplacesPendingVisual(type)) return false;
  uint32_t marker = 0;
  if (root["execute_at_ms"].is<uint32_t>()) marker = root["execute_at_ms"].as<uint32_t>();
  else if (root["sync_timebase"].is<uint32_t>()) marker = root["sync_timebase"].as<uint32_t>();
  else return false;

  const uint32_t now = millis();
  const bool hasRecentMarker = loomxLatestVisualCommandReceivedMs != 0 &&
                               (uint32_t)(now - loomxLatestVisualCommandReceivedMs) < LOOMX_VISUAL_STALE_WINDOW_MS;
  if (hasRecentMarker && (int32_t)(marker - loomxLatestVisualCommandMarker) < 0) return true;

  loomxLatestVisualCommandMarker = marker;
  loomxLatestVisualCommandReceivedMs = now;
  return false;
}

static uint32_t loomxRemainingDelay(uint32_t targetMs)
{
  if (targetMs == 0) return 0;
  const uint32_t now = millis();
  if ((int32_t)(targetMs - now) <= 0) return 0;
  return targetMs - now;
}

#ifdef ARDUINO_ARCH_ESP32
struct LoomxScheduledAction {
  char* body = nullptr;
  uint32_t executeAtMs = 0;
  uint32_t sequenceGeneration = 0;
  uint32_t visualGeneration = 0;
  LoomxScheduledAction* next = nullptr;
};

struct LoomxQueuedAction {
  char* body = nullptr;
  bool replaceVisual = false;
  uint32_t queueVisualGeneration = 0;
  LoomxQueuedAction* next = nullptr;
};

static LoomxScheduledAction* loomxScheduledActions = nullptr;
static LoomxQueuedAction* loomxActionQueueHead = nullptr;
static LoomxQueuedAction* loomxActionQueueTail = nullptr;
static volatile uint32_t loomxQueueVisualGeneration = 0;
static uint8_t loomxActionQueueSize = 0;
static portMUX_TYPE loomxActionQueueMux = portMUX_INITIALIZER_UNLOCKED;

static void freeLoomxScheduledAction(LoomxScheduledAction* action)
{
  if (!action) return;
  free(action->body);
  delete action;
}

static bool scheduleLoomxAction(const String& body, uint32_t delayMs, uint32_t sequenceGeneration, uint32_t visualGeneration)
{
  LoomxScheduledAction* action = new (std::nothrow) LoomxScheduledAction {};
  if (!action) return false;

  action->body = static_cast<char*>(malloc(body.length() + 1));
  if (!action->body) {
    delete action;
    return false;
  }

  memcpy(action->body, body.c_str(), body.length() + 1);
  action->executeAtMs = millis() + delayMs;
  action->sequenceGeneration = sequenceGeneration;
  action->visualGeneration = visualGeneration;

  if (!loomxScheduledActions || (int32_t)(action->executeAtMs - loomxScheduledActions->executeAtMs) < 0) {
    action->next = loomxScheduledActions;
    loomxScheduledActions = action;
    return true;
  }

  LoomxScheduledAction* cursor = loomxScheduledActions;
  while (cursor->next && (int32_t)(action->executeAtMs - cursor->next->executeAtMs) >= 0) cursor = cursor->next;
  action->next = cursor->next;
  cursor->next = action;
  return true;
}
#endif

static bool applyOrScheduleLoomxAction(JsonObject root, const String& body)
{
  const String type = root["type"] | "";
  if (loomxVisualActionIsStale(root, type)) return true;
  if (loomxActionCancelsSequence(root, type)) loomxCancelScheduledSequences();
  const uint32_t sequenceGeneration = loomxSequenceScheduleGeneration;
  const uint32_t visualGeneration = loomxRegisterVisualAction(type);
  uint32_t delayMs = 0;
  bool absoluteScheduleUsed = false;
  if (!root["execute_at_ms"].isNull() &&
      (Loomx::getRole() == Loomx::Role::Parent || Loomx::TimeSync::isSynced())) {
    const uint32_t localTargetMs = Loomx::TimeSync::localFor(root["execute_at_ms"].as<uint32_t>());
    delayMs = loomxRemainingDelay(localTargetMs);
    absoluteScheduleUsed = true;
  }
  if (!absoluteScheduleUsed) {
    if (root["execute_delay_ms"].is<uint32_t>()) delayMs = root["execute_delay_ms"].as<uint32_t>();
    else if (root["exec_delay_ms"].is<uint32_t>()) delayMs = root["exec_delay_ms"].as<uint32_t>();
  }

#ifdef ARDUINO_ARCH_ESP32
  if (delayMs >= LOOMX_SYNC_MIN_DELAY_MS && loomxActionShouldSync(type)) return scheduleLoomxAction(body, delayMs, sequenceGeneration, visualGeneration);
#else
  (void)body;
#endif

  return applyLoomxAction(root);
}

#ifdef ARDUINO_ARCH_ESP32
static void freeLoomxQueuedAction(LoomxQueuedAction* action)
{
  if (!action) return;
  free(action->body);
  delete action;
}

static bool queueLoomxAction(JsonObject root, const String& body, String& error)
{
  if (body.length() == 0 || body.length() >= LOOMX_ACTION_JSON_CAPACITY) {
    error = F("payload_too_large");
    return false;
  }

  const String type = root["type"] | "";
  const bool replaceVisual = loomxActionReplacesPendingVisual(type);
  const bool cancelsSequence = loomxActionCancelsSequence(root, type);
  LoomxQueuedAction* action = new (std::nothrow) LoomxQueuedAction {};
  if (!action) {
    error = F("queue_alloc_failed");
    return false;
  }

  action->body = static_cast<char*>(malloc(body.length() + 1));
  if (!action->body) {
    delete action;
    error = F("queue_alloc_failed");
    return false;
  }
  memcpy(action->body, body.c_str(), body.length() + 1);
  action->replaceVisual = replaceVisual;

  LoomxQueuedAction* purge = nullptr;
  portENTER_CRITICAL(&loomxActionQueueMux);
  if (cancelsSequence) loomxSequenceScheduleGeneration++;
  if (replaceVisual) {
    loomxVisualScheduleGeneration++;
    action->queueVisualGeneration = ++loomxQueueVisualGeneration;

    LoomxQueuedAction* keptHead = nullptr;
    LoomxQueuedAction* keptTail = nullptr;
    LoomxQueuedAction* cursor = loomxActionQueueHead;
    loomxActionQueueHead = nullptr;
    loomxActionQueueTail = nullptr;
    loomxActionQueueSize = 0;
    while (cursor) {
      LoomxQueuedAction* next = cursor->next;
      cursor->next = nullptr;
      if (cursor->replaceVisual) {
        cursor->next = purge;
        purge = cursor;
      } else {
        if (keptTail) keptTail->next = cursor;
        else keptHead = cursor;
        keptTail = cursor;
        loomxActionQueueSize++;
      }
      cursor = next;
    }
    loomxActionQueueHead = keptHead;
    loomxActionQueueTail = keptTail;
  }

  if (loomxActionQueueSize >= LOOMX_ACTION_QUEUE_MAX) {
    portEXIT_CRITICAL(&loomxActionQueueMux);
    while (purge) {
      LoomxQueuedAction* next = purge->next;
      freeLoomxQueuedAction(purge);
      purge = next;
    }
    freeLoomxQueuedAction(action);
    error = F("queue_full");
    return false;
  }

  if (loomxActionQueueTail) loomxActionQueueTail->next = action;
  else loomxActionQueueHead = action;
  loomxActionQueueTail = action;
  loomxActionQueueSize++;
  portEXIT_CRITICAL(&loomxActionQueueMux);

  while (purge) {
    LoomxQueuedAction* next = purge->next;
    freeLoomxQueuedAction(purge);
    purge = next;
  }
  return true;
}

static LoomxQueuedAction* popLoomxQueuedAction()
{
  portENTER_CRITICAL(&loomxActionQueueMux);
  LoomxQueuedAction* action = loomxActionQueueHead;
  if (action) {
    loomxActionQueueHead = action->next;
    if (!loomxActionQueueHead) loomxActionQueueTail = nullptr;
    if (loomxActionQueueSize > 0) loomxActionQueueSize--;
    action->next = nullptr;
  }
  portEXIT_CRITICAL(&loomxActionQueueMux);
  return action;
}

static void applyLoomxQueuedAction(LoomxQueuedAction* action)
{
  if (!action) return;
  if (action->replaceVisual && action->queueVisualGeneration != loomxQueueVisualGeneration) {
    freeLoomxQueuedAction(action);
    return;
  }

  DynamicJsonDocument doc(LOOMX_ACTION_JSON_CAPACITY);
  if (action->body && !deserializeJson(doc, action->body)) {
    applyOrScheduleLoomxAction(doc.as<JsonObject>(), String(action->body));
  }
  freeLoomxQueuedAction(action);
}

static void processLoomxScheduledActions()
{
  const uint32_t now = millis();
  while (loomxScheduledActions && (int32_t)(loomxScheduledActions->executeAtMs - now) <= 0) {
    LoomxScheduledAction* action = loomxScheduledActions;
    loomxScheduledActions = action->next;
    action->next = nullptr;

    DynamicJsonDocument doc(LOOMX_ACTION_JSON_CAPACITY);
    if (action->body && !deserializeJson(doc, action->body)) {
      JsonObject root = doc.as<JsonObject>();
      const String type = root["type"] | "";
      const bool sequenceCurrent = !loomxActionIsSequence(type) || action->sequenceGeneration == loomxSequenceScheduleGeneration;
      const bool visualCurrent = !loomxActionReplacesPendingVisual(type) || action->visualGeneration == loomxVisualScheduleGeneration;
      if (sequenceCurrent && visualCurrent) applyLoomxAction(root);
    }

    freeLoomxScheduledAction(action);
  }
}

void handleLoomxActions()
{
  for (uint8_t i = 0; i < 4; i++) {
    LoomxQueuedAction* action = popLoomxQueuedAction();
    if (!action) break;
    applyLoomxQueuedAction(action);
  }
  processLoomxScheduledActions();
}

static bool loomxActionClientAllowed(const IPAddress& ip)
{
  return inLocalSubnet(ip);
}

static bool loomxApplyActionBody(const char* data, size_t len, String& error)
{
  if (!data || len == 0) {
    error = F("empty_payload");
    return false;
  }
  if (len >= LOOMX_ACTION_JSON_CAPACITY) {
    error = F("payload_too_large");
    return false;
  }

  String body;
  body.reserve(len + 1);
  for (size_t i = 0; i < len; i++) body += data[i];

  DynamicJsonDocument doc(LOOMX_ACTION_JSON_CAPACITY);
  const DeserializationError parseError = deserializeJson(doc, body);
  if (parseError) {
    error = F("invalid_json");
    return false;
  }

  return queueLoomxAction(doc.as<JsonObject>(), body, error);
}

static String loomxJsonStatus(bool ok, const String& error = "")
{
  String payload = F("{\"ok\":");
  payload += ok ? F("true") : F("false");
  if (error.length()) {
    payload += F(",\"error\":");
    appendLoomxJsonString(payload, error);
  }
  payload += '}';
  return payload;
}

static bool loomxReadByteWithTimeout(WiFiClient& client, uint8_t& value, uint32_t timeoutMs)
{
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeoutMs) {
    if (client.available()) {
      const int raw = client.read();
      if (raw >= 0) {
        value = static_cast<uint8_t>(raw);
        return true;
      }
    }
    if (!client.connected() && !client.available()) return false;
    delay(1);
  }
  return false;
}

static bool loomxReadExact(WiFiClient& client, uint8_t* out, size_t len, uint32_t timeoutMs)
{
  for (size_t i = 0; i < len; i++) {
    if (!loomxReadByteWithTimeout(client, out[i], timeoutMs)) return false;
  }
  return true;
}

static bool loomxReadTcpJsonPayload(WiFiClient& client, String& body)
{
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 350 && body.length() < LOOMX_ACTION_JSON_CAPACITY - 1) {
    while (client.available()) {
      const int raw = client.read();
      if (raw < 0) break;
      const char c = static_cast<char>(raw);
      if (c == '\n') return body.length() > 0;
      if (c != '\r') body += c;
    }
    if (!client.connected() && !client.available()) break;
    delay(1);
  }
  return body.length() > 0;
}

static WiFiUDP loomxActionUdp;
static TaskHandle_t loomxUdpActionTaskHandle = nullptr;
static WiFiServer loomxActionTcpServer(LOOMX_ACTION_TCP_PORT, 4);
static TaskHandle_t loomxTcpActionTaskHandle = nullptr;
static WiFiServer loomxActionMqttServer(LOOMX_ACTION_MQTT_PORT, 2);
static TaskHandle_t loomxMqttActionTaskHandle = nullptr;

static void loomxUdpActionTask(void*)
{
  loomxActionUdp.begin(LOOMX_ACTION_UDP_PORT);

  for (;;) {
    const int packetSize = loomxActionUdp.parsePacket();
    if (packetSize > 0) {
      if (packetSize < static_cast<int>(LOOMX_ACTION_JSON_CAPACITY) &&
          loomxActionClientAllowed(loomxActionUdp.remoteIP())) {
        char buffer[LOOMX_ACTION_JSON_CAPACITY];
        const int len = loomxActionUdp.read(reinterpret_cast<uint8_t*>(buffer), sizeof(buffer) - 1);
        if (len > 0) {
          buffer[len] = '\0';
          String error;
          loomxApplyActionBody(buffer, len, error);
        }
      } else {
        while (loomxActionUdp.available()) loomxActionUdp.read();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static void loomxTcpActionTask(void*)
{
  loomxActionTcpServer.begin();
  loomxActionTcpServer.setNoDelay(true);

  for (;;) {
    WiFiClient client = loomxActionTcpServer.available();
    if (client) {
      client.setNoDelay(true);
      client.setTimeout(1);
      String error;
      bool ok = false;
      if (!loomxActionClientAllowed(client.remoteIP())) {
        error = F("client_not_local");
      } else {
        String body;
        if (loomxReadTcpJsonPayload(client, body)) ok = loomxApplyActionBody(body.c_str(), body.length(), error);
        else error = F("empty_payload");
      }
      const String ack = loomxJsonStatus(ok, error);
      client.print(ack);
      client.print('\n');
      client.flush();
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static uint8_t loomxMqttEncodeLength(uint32_t len, uint8_t* out)
{
  uint8_t index = 0;
  do {
    uint8_t encoded = len % 128;
    len /= 128;
    if (len > 0) encoded |= 128;
    out[index++] = encoded;
  } while (len > 0 && index < 4);
  return index;
}

static bool loomxMqttReadLength(WiFiClient& client, uint32_t& len)
{
  len = 0;
  uint32_t multiplier = 1;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t encoded = 0;
    if (!loomxReadByteWithTimeout(client, encoded, 350)) return false;
    len += (encoded & 127) * multiplier;
    if ((encoded & 128) == 0) return true;
    multiplier *= 128;
  }
  return false;
}

static bool loomxMqttSkipPayload(WiFiClient& client, uint32_t len)
{
  uint8_t value = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (!loomxReadByteWithTimeout(client, value, 350)) return false;
  }
  return true;
}

static bool loomxMqttHandlePublish(WiFiClient& client, uint8_t header, uint32_t remainingLen, String& error)
{
  uint8_t topicLenRaw[2] = {0, 0};
  if (remainingLen < 2 || !loomxReadExact(client, topicLenRaw, sizeof(topicLenRaw), 350)) {
    error = F("mqtt_bad_publish");
    return false;
  }

  uint32_t consumed = 2;
  const uint16_t topicLen = (uint16_t(topicLenRaw[0]) << 8) | uint16_t(topicLenRaw[1]);
  if (topicLen > 64 || remainingLen < consumed + topicLen) {
    error = F("mqtt_bad_topic");
    return false;
  }

  char topic[65] = {0};
  if (!loomxReadExact(client, reinterpret_cast<uint8_t*>(topic), topicLen, 350)) {
    error = F("mqtt_topic_read_failed");
    return false;
  }
  consumed += topicLen;

  const uint8_t qos = (header >> 1) & 0x03;
  if (qos > 0) {
    uint8_t packetId[2] = {0, 0};
    if (remainingLen < consumed + 2 || !loomxReadExact(client, packetId, sizeof(packetId), 350)) {
      error = F("mqtt_packet_id_failed");
      return false;
    }
    consumed += 2;
  }

  const uint32_t payloadLen = remainingLen - consumed;
  if (payloadLen == 0 || payloadLen >= LOOMX_ACTION_JSON_CAPACITY) {
    error = F("mqtt_payload_too_large");
    return false;
  }

  String body;
  body.reserve(payloadLen + 1);
  for (uint32_t i = 0; i < payloadLen; i++) {
    uint8_t value = 0;
    if (!loomxReadByteWithTimeout(client, value, 350)) {
      error = F("mqtt_payload_read_failed");
      return false;
    }
    body += static_cast<char>(value);
  }

  if (strcmp(topic, "loomx/action") != 0) {
    error = F("mqtt_ignored_topic");
    return false;
  }

  return loomxApplyActionBody(body.c_str(), body.length(), error);
}

static void loomxMqttActionTask(void*)
{
  loomxActionMqttServer.begin();
  loomxActionMqttServer.setNoDelay(true);

  for (;;) {
    WiFiClient client = loomxActionMqttServer.available();
    if (client) {
      client.setNoDelay(true);
      client.setTimeout(1);
      if (!loomxActionClientAllowed(client.remoteIP())) {
        client.stop();
        vTaskDelay(pdMS_TO_TICKS(5));
        continue;
      }

      bool connected = false;
      for (;;) {
        uint8_t header = 0;
        uint32_t remainingLen = 0;
        if (!loomxReadByteWithTimeout(client, header, 700) || !loomxMqttReadLength(client, remainingLen)) break;

        const uint8_t packetType = header >> 4;
        if (packetType == 1) {
          if (!loomxMqttSkipPayload(client, remainingLen)) break;
          const uint8_t connack[4] = {0x20, 0x02, 0x00, 0x00};
          client.write(connack, sizeof(connack));
          connected = true;
        } else if (packetType == 3 && connected) {
          String error;
          loomxMqttHandlePublish(client, header, remainingLen, error);
        } else if (packetType == 14) {
          break;
        } else if (!loomxMqttSkipPayload(client, remainingLen)) {
          break;
        }

        if (!client.connected() && !client.available()) break;
      }
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

#ifdef WLED_ENABLE_WEBSOCKETS
static AsyncWebSocket loomxActionWs("/api/loomx/ws");
static bool loomxActionWsRegistered = false;

static void loomxActionWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
  if (type != WS_EVT_DATA || !client) return;
  AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
  if (!info || info->opcode != WS_TEXT || !info->final || info->index != 0 || info->len != len) {
    client->text(loomxJsonStatus(false, F("fragmented_ws_unsupported")));
    return;
  }
  if (!loomxActionClientAllowed(client->remoteIP())) {
    client->text(loomxJsonStatus(false, F("client_not_local")));
    client->close();
    return;
  }

  String error;
  const bool ok = loomxApplyActionBody(reinterpret_cast<const char*>(data), len, error);
  client->text(loomxJsonStatus(ok, error));
}
#endif

static void startLoomxActionTransportListeners()
{
  if (!loomxUdpActionTaskHandle) {
    xTaskCreatePinnedToCore(loomxUdpActionTask, "loomx_udp_rx", 8192, nullptr, 1, &loomxUdpActionTaskHandle, 1);
  }
  if (!loomxTcpActionTaskHandle) {
    xTaskCreatePinnedToCore(loomxTcpActionTask, "loomx_tcp_rx", 6144, nullptr, 1, &loomxTcpActionTaskHandle, 1);
  }
  if (!loomxMqttActionTaskHandle) {
    xTaskCreatePinnedToCore(loomxMqttActionTask, "loomx_mqtt_rx", 6144, nullptr, 1, &loomxMqttActionTaskHandle, 1);
  }
#ifdef WLED_ENABLE_WEBSOCKETS
  if (!loomxActionWsRegistered) {
    loomxActionWs.onEvent(loomxActionWsEvent);
    server.addHandler(&loomxActionWs);
    loomxActionWsRegistered = true;
  }
#endif
}

static bool sendLoomxActionViaUdp(const IPAddress& ip, const String& body, String& error)
{
  WiFiUDP udp;
  if (!udp.beginPacket(ip, LOOMX_ACTION_UDP_PORT)) {
    error = F("udp_begin_failed");
    return false;
  }
  const size_t written = udp.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  if (!udp.endPacket() || written != body.length()) {
    error = F("udp_send_failed");
    return false;
  }
  return true;
}

static bool readLoomxLineAck(WiFiClient& client, String& ack, uint32_t timeoutMs)
{
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeoutMs && ack.length() < 160) {
    while (client.available()) {
      const int raw = client.read();
      if (raw < 0) break;
      const char c = static_cast<char>(raw);
      if (c == '\n') return ack.length() > 0;
      if (c != '\r') ack += c;
    }
    if (!client.connected() && !client.available()) break;
    delay(1);
  }
  return ack.length() > 0;
}

static bool sendLoomxActionViaTcp(const IPAddress& ip, const String& body, String& error)
{
  WiFiClient client;
  client.setTimeout(1);
  if (!client.connect(ip, LOOMX_ACTION_TCP_PORT, 500)) {
    error = F("tcp_connect_failed");
    return false;
  }
  client.setNoDelay(true);
  const size_t written = client.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  client.write('\n');
  client.flush();
  if (written != body.length()) {
    error = F("tcp_write_failed");
    client.stop();
    return false;
  }

  String ack;
  const bool ackRead = readLoomxLineAck(client, ack, 450);
  client.stop();
  if (!ackRead || ack.indexOf(F("\"ok\":true")) < 0) {
    error = ackRead ? ack : String(F("tcp_no_ack"));
    return false;
  }
  return true;
}

static bool readLoomxHttpHeaders(WiFiClient& client, String& response, uint32_t timeoutMs)
{
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < timeoutMs && response.length() < 900) {
    while (client.available()) {
      const int raw = client.read();
      if (raw < 0) break;
      response += static_cast<char>(raw);
      if (response.endsWith(F("\r\n\r\n"))) return true;
    }
    if (!client.connected() && !client.available()) break;
    delay(1);
  }
  return response.endsWith(F("\r\n\r\n"));
}

static bool writeLoomxWebSocketTextFrame(WiFiClient& client, const String& body)
{
  if (body.length() > 65535) return false;

  uint8_t header[8];
  size_t headerLen = 0;
  header[headerLen++] = 0x81;
  if (body.length() <= 125) {
    header[headerLen++] = 0x80 | static_cast<uint8_t>(body.length());
  } else {
    header[headerLen++] = 0x80 | 126;
    header[headerLen++] = (body.length() >> 8) & 0xFF;
    header[headerLen++] = body.length() & 0xFF;
  }

  uint8_t mask[4] = {
    static_cast<uint8_t>(random(0, 256)),
    static_cast<uint8_t>(random(0, 256)),
    static_cast<uint8_t>(random(0, 256)),
    static_cast<uint8_t>(random(0, 256))
  };
  memcpy(header + headerLen, mask, sizeof(mask));
  headerLen += sizeof(mask);

  if (client.write(header, headerLen) != headerLen) return false;
  uint8_t chunk[128];
  size_t offset = 0;
  while (offset < body.length()) {
    const size_t count = min(sizeof(chunk), body.length() - offset);
    for (size_t i = 0; i < count; i++) chunk[i] = body[offset + i] ^ mask[(offset + i) & 0x03];
    if (client.write(chunk, count) != count) return false;
    offset += count;
  }
  client.flush();
  return true;
}

static bool sendLoomxActionViaWebSocket(const IPAddress& ip, const String& body, String& error)
{
#ifndef WLED_ENABLE_WEBSOCKETS
  (void)ip;
  (void)body;
  error = F("websocket_disabled");
  return false;
#else
  WiFiClient client;
  client.setTimeout(1);
  if (!client.connect(ip, 80, 600)) {
    error = F("ws_connect_failed");
    return false;
  }
  client.setNoDelay(true);
  client.print(F("GET /api/loomx/ws HTTP/1.1\r\nHost: "));
  client.print(ip);
  client.print(F("\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"));

  String response;
  if (!readLoomxHttpHeaders(client, response, 700) || response.indexOf(F(" 101 ")) < 0) {
    error = F("ws_handshake_failed");
    client.stop();
    return false;
  }

  if (!writeLoomxWebSocketTextFrame(client, body)) {
    error = F("ws_write_failed");
    client.stop();
    return false;
  }
  client.stop();
  return true;
#endif
}

static bool writeLoomxMqttHeader(WiFiClient& client, uint8_t packetType, uint32_t remainingLen)
{
  uint8_t encoded[5];
  encoded[0] = packetType;
  const uint8_t lenBytes = loomxMqttEncodeLength(remainingLen, encoded + 1);
  return client.write(encoded, 1 + lenBytes) == size_t(1 + lenBytes);
}

static bool sendLoomxActionViaMqtt(const IPAddress& ip, const String& body, String& error)
{
  static const char topic[] = "loomx/action";
  static const char clientId[] = "loomx-parent";
  WiFiClient client;
  client.setTimeout(1);
  if (!client.connect(ip, LOOMX_ACTION_MQTT_PORT, 600)) {
    error = F("mqtt_connect_failed");
    return false;
  }
  client.setNoDelay(true);

  const uint16_t clientIdLen = strlen(clientId);
  const uint32_t connectRemaining = 10 + 2 + clientIdLen;
  if (!writeLoomxMqttHeader(client, 0x10, connectRemaining)) {
    error = F("mqtt_connect_write_failed");
    client.stop();
    return false;
  }
  const uint8_t variableHeader[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x0A};
  const uint8_t clientIdHeader[] = {uint8_t(clientIdLen >> 8), uint8_t(clientIdLen & 0xFF)};
  client.write(variableHeader, sizeof(variableHeader));
  client.write(clientIdHeader, sizeof(clientIdHeader));
  client.write(reinterpret_cast<const uint8_t*>(clientId), clientIdLen);
  client.flush();

  uint8_t connack[4] = {0, 0, 0, 0};
  if (!loomxReadExact(client, connack, sizeof(connack), 700) ||
      connack[0] != 0x20 || connack[1] != 0x02 || connack[3] != 0x00) {
    error = F("mqtt_connack_failed");
    client.stop();
    return false;
  }

  const uint16_t topicLen = strlen(topic);
  const uint32_t publishRemaining = 2 + topicLen + body.length();
  if (!writeLoomxMqttHeader(client, 0x30, publishRemaining)) {
    error = F("mqtt_publish_header_failed");
    client.stop();
    return false;
  }
  const uint8_t topicHeader[] = {uint8_t(topicLen >> 8), uint8_t(topicLen & 0xFF)};
  client.write(topicHeader, sizeof(topicHeader));
  client.write(reinterpret_cast<const uint8_t*>(topic), topicLen);
  const size_t written = client.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  const uint8_t disconnect[] = {0xE0, 0x00};
  client.write(disconnect, sizeof(disconnect));
  client.flush();
  client.stop();

  if (written != body.length()) {
    error = F("mqtt_publish_write_failed");
    return false;
  }
  return true;
}

struct LoomxConnectedChild {
  uint8_t mac[6];
  IPAddress ip;
  int8_t rssi;
};

struct LoomxForwardPost {
  IPAddress ip;
  char* body = nullptr;
  uint32_t forwardGeneration = 0;
};

static void loomxForwardPostTask(void* param)
{
  LoomxForwardPost* post = reinterpret_cast<LoomxForwardPost*>(param);
  if (!post) {
    vTaskDelete(nullptr);
    return;
  }

  const bool current = post->forwardGeneration == 0 || post->forwardGeneration == loomxForwardScheduleGeneration;
  if (current && post->body) {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(1200);
    if (http.begin(client, String(F("http://")) + post->ip.toString() + F("/api/loomx/action"))) {
      http.addHeader(F("Content-Type"), FPSTR(CONTENT_TYPE_JSON));
      http.POST(reinterpret_cast<uint8_t*>(post->body), strlen(post->body));
      http.end();
    }
  }

  free(post->body);
  delete post;
  vTaskDelete(nullptr);
}

static bool queueLoomxForwardPost(const IPAddress& ip, const String& body, uint32_t forwardGeneration)
{
  LoomxForwardPost* post = new (std::nothrow) LoomxForwardPost {};
  if (!post) return false;

  post->body = static_cast<char*>(malloc(body.length() + 1));
  if (!post->body) {
    delete post;
    return false;
  }

  memcpy(post->body, body.c_str(), body.length() + 1);
  post->ip = ip;
  post->forwardGeneration = forwardGeneration;
  if (xTaskCreatePinnedToCore(loomxForwardPostTask, "loomx_post", 7168, post, 1, nullptr, 1) == pdPASS) {
    return true;
  }

  free(post->body);
  delete post;
  return false;
}

static bool sendLoomxActionViaHttp(const IPAddress& ip, const String& body, bool syncAction, uint32_t forwardGeneration, String& error)
{
  if (syncAction) {
    if (queueLoomxForwardPost(ip, body, forwardGeneration)) return true;
    error = F("http_queue_failed");
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(800);
  if (!http.begin(client, String(F("http://")) + ip.toString() + F("/api/loomx/action"))) {
    error = F("http_begin_failed");
    return false;
  }
  http.addHeader(F("Content-Type"), FPSTR(CONTENT_TYPE_JSON));
  const int code = http.POST(body);
  const String response = code > 0 ? http.getString() : "";
  http.end();

  if (code >= 200 && code < 300) return true;
  error = code > 0 ? String(F("http_")) + code + F(" ") + response.substring(0, 80) : http.errorToString(code);
  return false;
}

static bool sendLoomxActionToChild(const IPAddress& ip, const String& body, bool syncAction, uint32_t forwardGeneration, Loomx::Storage::TransportType transport, String& error)
{
  switch (transport) {
    case Loomx::Storage::TransportType::Udp:
      return sendLoomxActionViaUdp(ip, body, error);
    case Loomx::Storage::TransportType::Tcp:
      return sendLoomxActionViaTcp(ip, body, error);
    case Loomx::Storage::TransportType::WebSocket:
      return sendLoomxActionViaWebSocket(ip, body, error);
    case Loomx::Storage::TransportType::Mqtt:
      return sendLoomxActionViaMqtt(ip, body, error);
    case Loomx::Storage::TransportType::Http:
    default:
      return sendLoomxActionViaHttp(ip, body, syncAction, forwardGeneration, error);
  }
}

struct LoomxChildOtaState {
  bool running = false;
  uint8_t total = 0;
  uint8_t completed = 0;
  uint8_t succeeded = 0;
  uint8_t failed = 0;
  uint32_t startedMs = 0;
  uint32_t finishedMs = 0;
  char firmware[32] = {0};
  char current[16] = {0};
  char error[96] = {0};
};

struct LoomxOtaReceiveContext {
  bool started = false;
  bool complete = false;
  String error;
};

static LoomxChildOtaState loomxChildOtaState;
static TaskHandle_t loomxChildOtaTaskHandle = nullptr;

class LoomxPartitionStream : public Stream {
public:
  LoomxPartitionStream(const esp_partition_t* partition, size_t size) : _partition(partition), _size(size) {}

  int available() override
  {
    const size_t remaining = _offset < _size ? _size - _offset : 0;
    return remaining > INT_MAX ? INT_MAX : static_cast<int>(remaining);
  }

  int read() override
  {
    uint8_t value = 0;
    return readBytes(reinterpret_cast<char*>(&value), 1) == 1 ? value : -1;
  }

  int peek() override
  {
    if (_offset >= _size) return -1;
    uint8_t value = 0;
    if (esp_partition_read(_partition, _offset, &value, 1) != ESP_OK) return -1;
    return value;
  }

  size_t readBytes(char* buffer, size_t length) override
  {
    if (!_partition || !buffer || _offset >= _size) return 0;
    const size_t toRead = min(length, _size - _offset);
    if (esp_partition_read(_partition, _offset, buffer, toRead) != ESP_OK) return 0;
    _offset += toRead;
    return toRead;
  }

  void flush() override {}
  size_t write(uint8_t) override { return 0; }

private:
  const esp_partition_t* _partition;
  const size_t _size;
  size_t _offset = 0;
};

class LoomxFirmwareMultipartStream : public Stream {
public:
  LoomxFirmwareMultipartStream(const esp_partition_t* partition, size_t imageSize, const String& boundary)
    : _partition(partition),
      _imageSize(imageSize),
      _prefix(String(F("--")) + boundary + F("\r\nContent-Disposition: form-data; name=\"update\"; filename=\"firmware.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n")),
      _suffix(String(F("\r\n--")) + boundary + F("--\r\n"))
  {}

  size_t totalSize() const
  {
    return _prefix.length() + _imageSize + _suffix.length();
  }

  int available() override
  {
    const size_t total = totalSize();
    const size_t remaining = _offset < total ? total - _offset : 0;
    return remaining > INT_MAX ? INT_MAX : static_cast<int>(remaining);
  }

  int read() override
  {
    uint8_t value = 0;
    return readBytes(reinterpret_cast<char*>(&value), 1) == 1 ? value : -1;
  }

  int peek() override
  {
    const size_t savedOffset = _offset;
    uint8_t value = 0;
    const size_t count = readBytes(reinterpret_cast<char*>(&value), 1);
    _offset = savedOffset;
    return count == 1 ? value : -1;
  }

  size_t readBytes(char* buffer, size_t length) override
  {
    if (!_partition || !buffer || length == 0) return 0;
    const size_t total = totalSize();
    size_t written = 0;

    while (written < length && _offset < total) {
      const size_t prefixLen = _prefix.length();
      const size_t imageStart = prefixLen;
      const size_t imageEnd = imageStart + _imageSize;

      if (_offset < prefixLen) {
        const size_t chunk = min(length - written, prefixLen - _offset);
        memcpy(buffer + written, _prefix.c_str() + _offset, chunk);
        _offset += chunk;
        written += chunk;
      } else if (_offset < imageEnd) {
        const size_t imageOffset = _offset - imageStart;
        const size_t chunk = min(length - written, imageEnd - _offset);
        if (esp_partition_read(_partition, imageOffset, buffer + written, chunk) != ESP_OK) break;
        _offset += chunk;
        written += chunk;
      } else {
        const size_t suffixOffset = _offset - imageEnd;
        const size_t chunk = min(length - written, _suffix.length() - suffixOffset);
        memcpy(buffer + written, _suffix.c_str() + suffixOffset, chunk);
        _offset += chunk;
        written += chunk;
      }
    }
    return written;
  }

  void flush() override {}
  size_t write(uint8_t) override { return 0; }

private:
  const esp_partition_t* _partition;
  const size_t _imageSize;
  const String _prefix;
  const String _suffix;
  size_t _offset = 0;
};

static bool probeLoomxChildRole(const IPAddress& ip)
{
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(300);
  if (!http.begin(client, String(F("http://")) + ip.toString() + F("/api/role"))) return false;
  const int code = http.GET();
  const String roleBody = code >= 200 && code < 300 ? http.getString() : "";
  http.end();
  return roleBody.indexOf(F("\"child\"")) >= 0;
}

static bool collectConnectedLoomxChildren(std::vector<LoomxConnectedChild>& children)
{
  wifi_sta_list_t stationList;
  tcpip_adapter_sta_list_t adapterList;
  if (esp_wifi_ap_get_sta_list(&stationList) != ESP_OK) return false;
  if (tcpip_adapter_get_sta_list(&stationList, &adapterList) != ESP_OK) return false;

  for (int i = 0; i < adapterList.num; i++) {
    const tcpip_adapter_sta_info_t& sta = adapterList.sta[i];
    IPAddress ip(sta.ip.addr);
    if (!probeLoomxChildRole(ip)) continue;

    LoomxConnectedChild child;
    memcpy(child.mac, sta.mac, sizeof(child.mac));
    child.ip = ip;
    child.rssi = stationList.sta[i].rssi;
    children.push_back(child);
  }
  return true;
}

static bool getRunningFirmwareImage(const esp_partition_t*& partition, size_t& imageSize, String& error)
{
  partition = esp_ota_get_running_partition();
  imageSize = 0;
  if (!partition) {
    error = F("no_running_partition");
    return false;
  }

  esp_image_header_t header;
  if (esp_partition_read(partition, 0, &header, sizeof(header)) != ESP_OK) {
    error = F("image_header_read_failed");
    return false;
  }
  if (header.magic != ESP_IMAGE_HEADER_MAGIC || header.segment_count == 0 || header.segment_count > ESP_IMAGE_MAX_SEGMENTS) {
    error = F("invalid_running_image");
    return false;
  }

  size_t offset = sizeof(esp_image_header_t);
  for (uint8_t i = 0; i < header.segment_count; i++) {
    esp_image_segment_header_t segment;
    if (offset + sizeof(segment) > partition->size || esp_partition_read(partition, offset, &segment, sizeof(segment)) != ESP_OK) {
      error = F("segment_header_read_failed");
      return false;
    }
    offset += sizeof(segment);
    if (segment.data_len > partition->size || offset + segment.data_len > partition->size) {
      error = F("segment_out_of_range");
      return false;
    }
    offset += segment.data_len;
  }

  imageSize = ((offset + 15) & ~static_cast<size_t>(15)) + 1;
  if (header.hash_appended) imageSize += 32;
  if (imageSize == 0 || imageSize > partition->size) {
    error = F("image_size_out_of_range");
    return false;
  }
  return true;
}

static bool sendFirmwareToChildViaWledUpdate(const IPAddress& ip, const esp_partition_t* partition, size_t imageSize, String& error)
{
  const String boundary = F("----LoomxFirmwareClone");
  LoomxFirmwareMultipartStream stream(partition, imageSize, boundary);
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(30000);
  if (!http.begin(client, String(F("http://")) + ip.toString() + F("/update"))) {
    error = F("connect_failed");
    return false;
  }

  http.addHeader(F("Content-Type"), String(F("multipart/form-data; boundary=")) + boundary);
  const int code = http.sendRequest("POST", &stream, stream.totalSize());
  const String body = code > 0 ? http.getString() : "";
  http.end();

  if (code >= 200 && code < 300) return true;
  error = code > 0 ? String(F("wled_update_failed_")) + code : http.errorToString(code);
  return false;
}

static bool sendFirmwareToChildViaLoomxOta(const IPAddress& ip, const esp_partition_t* partition, size_t imageSize, String& error)
{
  LoomxPartitionStream stream(partition, imageSize);
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(30000);
  if (!http.begin(client, String(F("http://")) + ip.toString() + F("/api/loomx/ota"))) {
    error = F("connect_failed");
    return false;
  }

  http.addHeader(F("Content-Type"), F("application/octet-stream"));
  http.addHeader(F("X-Loomx-Firmware-Clone"), F("1"));
  http.addHeader(F("X-Loomx-Firmware-Version"), versionString);
  const int code = http.sendRequest("POST", &stream, imageSize);
  const String body = code > 0 ? http.getString() : "";
  http.end();

  if (code >= 200 && code < 300) {
    DynamicJsonDocument response(256);
    if (!deserializeJson(response, body) && (response["ok"] | false)) return true;
    error = F("loomx_ota_unexpected_response");
    return false;
  }
  if (code == 404 || code == 405) error = F("loomx_ota_unavailable");
  else error = code > 0 ? String(F("loomx_ota_failed_")) + code : http.errorToString(code);
  return false;
}

static bool sendFirmwareToChild(const IPAddress& ip, const esp_partition_t* partition, size_t imageSize, String& error)
{
  String loomxError;
  if (sendFirmwareToChildViaLoomxOta(ip, partition, imageSize, loomxError)) return true;

  if (loomxError != F("loomx_ota_unavailable")) {
    error = loomxError;
    return false;
  }

  String updateError;
  if (sendFirmwareToChildViaWledUpdate(ip, partition, imageSize, updateError)) return true;

  error = loomxError + F("; ") + updateError;
  return false;
}

static void resetLoomxChildOtaState()
{
  loomxChildOtaState.running = true;
  loomxChildOtaState.total = 0;
  loomxChildOtaState.completed = 0;
  loomxChildOtaState.succeeded = 0;
  loomxChildOtaState.failed = 0;
  loomxChildOtaState.startedMs = millis();
  loomxChildOtaState.finishedMs = 0;
  strlcpy(loomxChildOtaState.firmware, versionString, sizeof(loomxChildOtaState.firmware));
  loomxChildOtaState.current[0] = '\0';
  loomxChildOtaState.error[0] = '\0';
}

static void loomxChildOtaTask(void*)
{
  resetLoomxChildOtaState();

  const esp_partition_t* partition = nullptr;
  size_t imageSize = 0;
  String error;
  if (!getRunningFirmwareImage(partition, imageSize, error)) {
    strlcpy(loomxChildOtaState.error, error.c_str(), sizeof(loomxChildOtaState.error));
    loomxChildOtaState.failed = 1;
    loomxChildOtaState.running = false;
    loomxChildOtaState.finishedMs = millis();
    loomxChildOtaTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  std::vector<LoomxConnectedChild> children;
  if (!collectConnectedLoomxChildren(children)) {
    strlcpy(loomxChildOtaState.error, "child_scan_failed", sizeof(loomxChildOtaState.error));
  }
  loomxChildOtaState.total = children.size() > 255 ? 255 : children.size();

  for (const LoomxConnectedChild& child : children) {
    strlcpy(loomxChildOtaState.current, child.ip.toString().c_str(), sizeof(loomxChildOtaState.current));
    error = "";
    if (sendFirmwareToChild(child.ip, partition, imageSize, error)) {
      loomxChildOtaState.succeeded++;
    } else {
      loomxChildOtaState.failed++;
      strlcpy(loomxChildOtaState.error, error.c_str(), sizeof(loomxChildOtaState.error));
    }
    loomxChildOtaState.completed++;
    delay(250);
  }

  loomxChildOtaState.current[0] = '\0';
  loomxChildOtaState.running = false;
  loomxChildOtaState.finishedMs = millis();
  loomxChildOtaTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

static void appendLoomxChildOtaStatusJson(String& payload)
{
  payload += F("{\"running\":");
  payload += loomxChildOtaState.running ? F("true") : F("false");
  payload += F(",\"total\":");
  payload += String(loomxChildOtaState.total);
  payload += F(",\"completed\":");
  payload += String(loomxChildOtaState.completed);
  payload += F(",\"succeeded\":");
  payload += String(loomxChildOtaState.succeeded);
  payload += F(",\"failed\":");
  payload += String(loomxChildOtaState.failed);
  payload += F(",\"firmware\":\"v");
  payload += loomxChildOtaState.firmware[0] ? loomxChildOtaState.firmware : versionString;
  payload += F("\",\"current\":");
  appendLoomxJsonString(payload, loomxChildOtaState.current);
  payload += F(",\"error\":");
  appendLoomxJsonString(payload, loomxChildOtaState.error);
  payload += '}';
}

static void failLoomxOtaReceive(LoomxOtaReceiveContext* context, const String& error)
{
  if (!context || context->error.length()) return;
  context->error = error;
  if (context->started && !context->complete) {
    Update.abort();
    strip.resume();
    UsermodManager::onUpdateBegin(false);
    #if WLED_WATCHDOG_TIMEOUT > 0
    WLED::instance().enableWatchdog();
    #endif
  }
}

static void cleanupLoomxOtaReceive(AsyncWebServerRequest* request, bool abortUpdate)
{
  LoomxOtaReceiveContext* context = reinterpret_cast<LoomxOtaReceiveContext*>(request->_tempObject);
  request->_tempObject = nullptr;
  if (abortUpdate && context && context->started && !context->complete) {
    Update.abort();
    strip.resume();
    UsermodManager::onUpdateBegin(false);
    #if WLED_WATCHDOG_TIMEOUT > 0
    WLED::instance().enableWatchdog();
    #endif
  }
  delete context;
}
#endif

static bool appendConnectedChildrenJson(String& payload)
{
#ifdef ARDUINO_ARCH_ESP32
  std::vector<LoomxConnectedChild> children;
  payload += '[';
  if (!collectConnectedLoomxChildren(children)) {
    payload += ']';
    return false;
  }

  for (size_t i = 0; i < children.size(); i++) {
    const LoomxConnectedChild& child = children[i];
    if (i > 0) payload += ',';
    payload += F("{\"id\":");
    appendLoomxJsonString(payload, loomxMacToString(child.mac));
    payload += F(",\"mac\":");
    appendLoomxJsonString(payload, loomxMacToString(child.mac));
    payload += F(",\"ip\":");
    appendLoomxJsonString(payload, child.ip.toString());
    payload += F(",\"rssi\":");
    payload += String(child.rssi);
    payload += F(",\"online\":true}");
  }
  payload += ']';
  return true;
#else
  payload += F("[]");
  return true;
#endif
}

static bool actionTargetsChild(JsonObject root, const String& childId)
{
  const char* targetId = root["target_id"] | "";
  if (targetId && strlen(targetId) > 0) return childId == String(targetId);

  const char* childTargetId = root["child_id"] | "";
  if (childTargetId && strlen(childTargetId) > 0) return childId == String(childTargetId);

  JsonArray targetIds = root["target_ids"].as<JsonArray>();
  if (!targetIds.isNull() && targetIds.size() > 0) {
    for (JsonVariant target : targetIds) {
      if (childId == String(target.as<const char*>())) return true;
    }
    return false;
  }

  return true;
}

struct LoomxForwardResult {
  uint8_t sent = 0;
  uint8_t attempted = 0;
  uint8_t targets = 0;
  uint8_t failed = 0;
  Loomx::Storage::TransportType transport = Loomx::Storage::TransportType::Http;
  String firstError;
  String firstErrorTarget;
  String firstErrorIp;
};

static LoomxForwardResult forwardLoomxActionToChildren(const String& body, JsonObject root, uint32_t syncTargetMs)
{
  LoomxForwardResult result;
#ifdef ARDUINO_ARCH_ESP32
  std::vector<LoomxConnectedChild> children;
  if (!collectConnectedLoomxChildren(children)) {
    result.firstError = F("child_scan_failed");
    return result;
  }

  result.transport = Loomx::Storage::readTransportType();
  const String type = root["type"] | "";
  const bool syncAction = syncTargetMs > 0 && loomxActionShouldSync(type);
  const uint32_t forwardGeneration = loomxActionReplacesPendingVisual(type) ? ++loomxForwardScheduleGeneration : 0;
  for (const LoomxConnectedChild& child : children) {
    const String childId = loomxMacToString(child.mac);
    if (!actionTargetsChild(root, childId)) continue;
    JsonObject routeOutputTargets = loomxOutputTargetsForAction(root);
    if (!routeOutputTargets.isNull() && routeOutputTargets[childId.c_str()].isNull()) continue;
    result.targets++;

    String postBody = body;
    const bool needsChildBody = syncAction || type == F("preset_apply");
    if (needsChildBody) {
      DynamicJsonDocument childDoc(LOOMX_ACTION_JSON_CAPACITY);
      if (!deserializeJson(childDoc, body)) {
        if (type == F("preset_apply")) {
          JsonObject states = root["preset"]["states"].as<JsonObject>();
          JsonVariant childState = states[childId.c_str()];
          if (!states.isNull() && !childState.isNull()) {
            childDoc["state"].set(childState);
            childDoc.remove("preset");
            childDoc.remove("target_ids");
          }
        }
        JsonObject outputTargets = loomxOutputTargetsForAction(root);
        if (!outputTargets.isNull()) {
          JsonVariant childOutputs = outputTargets[childId.c_str()];
          childDoc.remove("output_targets");
          if (!childOutputs.isNull()) childDoc["target_outputs"].set(childOutputs);
        }
        if (syncAction) childDoc["execute_delay_ms"] = loomxRemainingDelay(syncTargetMs);
        postBody = "";
        serializeJson(childDoc, postBody);
      }
    }

    result.attempted++;
    String error;
    if (sendLoomxActionToChild(child.ip, postBody, syncAction, forwardGeneration, result.transport, error)) {
      result.sent++;
    } else {
      result.failed++;
      if (!result.firstError.length()) {
        result.firstError = error.length() ? error : String(F("send_failed"));
        result.firstErrorTarget = childId;
        result.firstErrorIp = child.ip.toString();
      }
    }
  }
#else
  (void)body;
  (void)root;
  (void)syncTargetMs;
#endif
  return result;
}

static void createLoomxApiHandlers()
{
#ifdef ARDUINO_ARCH_ESP32
  startLoomxActionTransportListeners();
#endif

  server.on(F("/api/role"), HTTP_GET, [](AsyncWebServerRequest *request) {
    const Loomx::Role role = Loomx::getRole();
    String payload = F("{\"role\":");
    if (role == Loomx::Role::Parent) payload += F("\"parent\"");
    else if (role == Loomx::Role::Child) payload += F("\"child\"");
    else payload += F("null");
    payload += '}';
    sendLoomxJson(request, payload);
  });

  AsyncCallbackJsonWebHandler* roleHandler = new AsyncCallbackJsonWebHandler(String(F("/api/role")), [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(256);
    const DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
    if (error) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_json\"}"), 400);
      return;
    }

    const char* roleRaw = doc["role"] | "";
    Loomx::Role role = Loomx::Role::Unprovisioned;
    if (!strcmp(roleRaw, "parent")) role = Loomx::Role::Parent;
    else if (!strcmp(roleRaw, "child")) role = Loomx::Role::Child;
    else if (doc["role"].isNull()) role = Loomx::Role::Unprovisioned;
    else {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_role\"}"), 400);
      return;
    }

    if (!Loomx::setRole(role)) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"nvs_write_failed\"}"), 500);
      return;
    }
    Loomx::setup();
    forceReconnect = true;
    sendLoomxJson(request, F("{\"ok\":true,\"reconnect\":true}"));
  }, 256);
  server.addHandler(roleHandler);

  server.on(F("/api/system/info"), HTTP_GET, [](AsyncWebServerRequest *request) {
    String payload = F("{\"firmware\":\"v");
    payload += versionString;
    payload += F("\",\"vid\":");
    payload += String(VERSION);
    payload += F(",\"uptime\":\"");
    payload += String(millis() / 1000 + rolloverMillis * 4294967);
    payload += F("s\",\"free_heap\":\"");
    payload += String(ESP.getFreeHeap() / 1024);
    payload += F(" KB\"}");
    sendLoomxJson(request, payload);
  });

  server.on(F("/api/network"), HTTP_GET, [](AsyncWebServerRequest *request) {
    String payload = F("{\"ssid\":");
    appendLoomxJsonString(payload, apSSID);
    payload += F(",\"password\":\"");
    payload += Loomx::Creds::LOOMX_AP_PASS;
    payload += F("\",\"hidden\":");
    payload += apHide ? F("true") : F("false");
    payload += F(",\"channel\":");
    if (apChannel > 0) payload += String(apChannel);
    else payload += F("\"auto\"");
    payload += '}';
    sendLoomxJson(request, payload);
  });

  AsyncCallbackJsonWebHandler* networkHandler = new AsyncCallbackJsonWebHandler(String(F("/api/network")), [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    const DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
    if (error) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_json\"}"), 400);
      return;
    }

    const char* ssid = doc["ssid"] | apSSID;
    if (ssid && strlen(ssid) > 0) strlcpy(apSSID, ssid, sizeof(apSSID));
    strlcpy(apPass, Loomx::Creds::LOOMX_AP_PASS, sizeof(apPass));
    apHide = doc["hidden"] | apHide;
    if (doc["channel"].is<int>()) apChannel = doc["channel"].as<int>();
    forceReconnect = true;
    sendLoomxJson(request, F("{\"ok\":true,\"password\":\"loomxleaf\",\"reconnect\":true}"));
  }, 512);
  server.addHandler(networkHandler);

  server.on(F("/api/loomx/transport"), HTTP_GET, [](AsyncWebServerRequest *request) {
    const auto transport = Loomx::Storage::readTransportType();
    String payload = F("{\"transport\":\"");
    payload += Loomx::Storage::transportTypeName(transport);
    payload += F("\",\"options\":[\"http\",\"udp\",\"tcp\",\"websocket\",\"mqtt\"],\"ports\":{\"udp\":");
    payload += String(LOOMX_ACTION_UDP_PORT);
    payload += F(",\"tcp\":");
    payload += String(LOOMX_ACTION_TCP_PORT);
    payload += F(",\"mqtt\":");
    payload += String(LOOMX_ACTION_MQTT_PORT);
    payload += F("},\"note\":\"Controller IPs stay on the existing AP subnet.\"}");
    sendLoomxJson(request, payload);
  });

  AsyncCallbackJsonWebHandler* transportHandler = new AsyncCallbackJsonWebHandler(String(F("/api/loomx/transport")), [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(256);
    const DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
    if (error) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_json\"}"), 400);
      return;
    }

    String transportRaw = doc["transport"] | "http";
    transportRaw.toLowerCase();
    if (transportRaw != F("http") &&
        transportRaw != F("udp") &&
        transportRaw != F("tcp") &&
        transportRaw != F("websocket") &&
        transportRaw != F("ws") &&
        transportRaw != F("mqtt")) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_transport\"}"), 400);
      return;
    }
    const auto transport = Loomx::Storage::transportTypeFromName(transportRaw);
    if (!Loomx::Storage::writeTransportType(transport)) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"nvs_write_failed\"}"), 500);
      return;
    }

    String payload = F("{\"ok\":true,\"transport\":\"");
    payload += Loomx::Storage::transportTypeName(transport);
    payload += F("\"}");
    sendLoomxJson(request, payload);
  }, 256);
  server.addHandler(transportHandler);

  server.on(F("/api/child/config"), HTTP_GET, [](AsyncWebServerRequest *request) {
    const auto type = Loomx::Storage::readControllerType();
    String payload = F("{\"controller_type\":\"");
    payload += Loomx::Storage::controllerTypeName(type);
    payload += F("\"}");
    sendLoomxJson(request, payload);
  });

  AsyncCallbackJsonWebHandler* childConfigHandler = new AsyncCallbackJsonWebHandler(String(F("/api/child/config")), [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(256);
    const DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
    if (error) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_json\"}"), 400);
      return;
    }

    const String controllerType = doc["controller_type"] | "VX4";
    if (!Loomx::Storage::writeControllerType(Loomx::Storage::controllerTypeFromName(controllerType))) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"nvs_write_failed\"}"), 500);
      return;
    }
    sendLoomxJson(request, F("{\"ok\":true}"));
  }, 256);
  server.addHandler(childConfigHandler);

  server.on(F("/api/children/scan"), HTTP_GET, [](AsyncWebServerRequest *request) {
    if (Loomx::getRole() != Loomx::Role::Parent) {
      sendLoomxJson(request, F("{\"available\":[],\"error\":\"not_parent\"}"), 409);
      return;
    }

    std::vector<Loomx::Discovery::ChildCandidate> candidates;
    const bool ok = Loomx::Discovery::scan(candidates);
    String payload = F("{\"available\":[");
    for (size_t i = 0; i < candidates.size(); i++) {
      if (i > 0) payload += ',';
      payload += F("{\"ssid\":");
      appendLoomxJsonString(payload, candidates[i].ssid);
      payload += F(",\"rssi\":");
      payload += String(candidates[i].rssi);
      payload += F(",\"bssid\":");
      appendLoomxJsonString(payload, loomxMacToString(candidates[i].bssid));
      payload += '}';
    }
    payload += F("],\"self_ssid\":");
    appendLoomxJsonString(payload, apSSID);
    payload += F(",\"ok\":");
    payload += ok ? F("true") : F("false");
    payload += '}';
    sendLoomxJson(request, payload);
  });

  server.on(F("/api/children/status"), HTTP_GET, [](AsyncWebServerRequest *request) {
    String payload = F("{\"children\":");
    appendConnectedChildrenJson(payload);
    payload += F(",\"self_ssid\":");
    appendLoomxJsonString(payload, apSSID);
    payload += '}';
    sendLoomxJson(request, payload);
  });

#ifdef ARDUINO_ARCH_ESP32
  server.on(F("/api/children/ota/status"), HTTP_GET, [](AsyncWebServerRequest *request) {
    String payload = F("{\"ok\":true,\"status\":");
    appendLoomxChildOtaStatusJson(payload);
    payload += '}';
    sendLoomxJson(request, payload);
  });

  server.on(F("/api/children/ota"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (Loomx::getRole() != Loomx::Role::Parent) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"not_parent\"}"), 409);
      return;
    }
    if (loomxChildOtaState.running || loomxChildOtaTaskHandle) {
      String payload = F("{\"ok\":true,\"started\":false,\"status\":");
      appendLoomxChildOtaStatusJson(payload);
      payload += '}';
      sendLoomxJson(request, payload);
      return;
    }

    resetLoomxChildOtaState();
    if (xTaskCreatePinnedToCore(loomxChildOtaTask, "loomx_child_ota", 8192, nullptr, 1, &loomxChildOtaTaskHandle, 1) != pdPASS) {
      loomxChildOtaState.running = false;
      strlcpy(loomxChildOtaState.error, "task_create_failed", sizeof(loomxChildOtaState.error));
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"task_create_failed\"}"), 500);
      return;
    }

    String payload = F("{\"ok\":true,\"started\":true,\"status\":");
    appendLoomxChildOtaStatusJson(payload);
    payload += '}';
    sendLoomxJson(request, payload);
  });

  server.on(F("/api/loomx/ota"), HTTP_POST, [](AsyncWebServerRequest *request) {
    LoomxOtaReceiveContext* context = reinterpret_cast<LoomxOtaReceiveContext*>(request->_tempObject);
    if (!context) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"missing_ota_context\"}"), 500);
      return;
    }

    if (context->error.length()) {
      String payload = F("{\"ok\":false,\"error\":");
      appendLoomxJsonString(payload, context->error);
      payload += '}';
      cleanupLoomxOtaReceive(request, false);
      sendLoomxJson(request, payload, 500);
      return;
    }

    if (!context->complete) {
      cleanupLoomxOtaReceive(request, true);
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"incomplete_upload\"}"), 400);
      return;
    }

    String payload = F("{\"ok\":true,\"reboot\":true,\"firmware\":\"v");
    payload += versionString;
    payload += F("\"}");
    cleanupLoomxOtaReceive(request, false);
    sendLoomxJson(request, payload);
  }, [](AsyncWebServerRequest*, const String&, size_t, uint8_t*, size_t, bool) {
  }, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    LoomxOtaReceiveContext* context = reinterpret_cast<LoomxOtaReceiveContext*>(request->_tempObject);
    if (!context) {
      context = new (std::nothrow) LoomxOtaReceiveContext {};
      request->_tempObject = context;
      request->onDisconnect([=]() { cleanupLoomxOtaReceive(request, true); });
    }
    if (!context || context->error.length()) return;

    if (index == 0) {
      if (Loomx::getRole() != Loomx::Role::Child) {
        failLoomxOtaReceive(context, F("not_child"));
        return;
      }
      IPAddress client = request->client()->remoteIP();
      if (!inSameSubnet(client) && !inLocalSubnet(client)) {
        failLoomxOtaReceive(context, F("client_not_local"));
        return;
      }
      if (total == 0 || total > ESP.getFreeSketchSpace()) {
        failLoomxOtaReceive(context, F("invalid_size"));
        return;
      }
      if (Update.isRunning()) {
        failLoomxOtaReceive(context, F("ota_busy"));
        return;
      }

      #if WLED_WATCHDOG_TIMEOUT > 0
      WLED::instance().disableWatchdog();
      #endif
      UsermodManager::onUpdateBegin(true);
      strip.suspend();
      strip.resetSegments();
      if (!Update.begin(total)) {
        failLoomxOtaReceive(context, Update.errorString());
        return;
      }
      context->started = true;
    }

    if (!context->started) {
      failLoomxOtaReceive(context, F("ota_not_started"));
      return;
    }
    if (Update.write(data, len) != len) {
      failLoomxOtaReceive(context, Update.errorString());
      return;
    }

    if (index + len >= total) {
      if (!Update.end(true)) {
        failLoomxOtaReceive(context, Update.errorString());
        return;
      }
      bootloopCheckOTA();
      context->complete = true;
      doReboot = true;
    }
  });
#endif

  AsyncCallbackJsonWebHandler* loomxSendHandler = new AsyncCallbackJsonWebHandler(String(F("/api/loomx/send")), [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(LOOMX_ACTION_JSON_CAPACITY);
    const DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
    if (error) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_json\"}"), 400);
      return;
    }

    uint8_t sent = 0;
    LoomxForwardResult forwardResult;
    const String messageType = doc["type"] | "";
    if (messageType == F("preset_apply") || messageType == F("stop_all")) doc["cancel_sequence"] = true;
    const bool localMasterAction = messageType == F("master_power") || messageType == F("master_brightness");
    const bool syncAction = Loomx::getRole() == Loomx::Role::Parent && loomxActionShouldSync(messageType);
    const uint32_t syncTargetMs = syncAction ? millis() + loomxSyncLeadMsForType(messageType) : 0;
    if (syncTargetMs > 0) {
      doc["execute_at_ms"] = syncTargetMs;
      doc["execute_delay_ms"] = loomxRemainingDelay(syncTargetMs);
      if (loomxActionUsesEffectTimebase(messageType)) doc["sync_timebase"] = syncTargetMs;
    }

    String body;
    serializeJson(doc, body);
    if (Loomx::getRole() == Loomx::Role::Parent) {
      forwardResult = forwardLoomxActionToChildren(body, doc.as<JsonObject>(), syncTargetMs);
      sent = forwardResult.sent;
      if (localMasterAction) {
        if (syncTargetMs > 0) {
          doc["execute_delay_ms"] = loomxRemainingDelay(syncTargetMs);
          body = "";
          serializeJson(doc, body);
        }
        String localError;
        if (queueLoomxAction(doc.as<JsonObject>(), body, localError)) sent++;
      }
    } else if (Loomx::getRole() == Loomx::Role::Child) {
      String localError;
      sent = queueLoomxAction(doc.as<JsonObject>(), body, localError) ? 1 : 0;
    }

    const bool ackRequired = messageType == F("child_config");
    const bool ok = sent > 0 || !ackRequired;
    String payload = F("{\"ok\":");
    payload += ok ? F("true") : F("false");
    payload += F(",\"sent\":");
    payload += String(sent);
    payload += F(",\"transport\":\"");
    payload += Loomx::Storage::transportTypeName(forwardResult.transport);
    payload += F("\",\"targets\":");
    payload += String(forwardResult.targets);
    payload += F(",\"attempted\":");
    payload += String(forwardResult.attempted);
    payload += F(",\"failed\":");
    payload += String(forwardResult.failed);
    if (forwardResult.firstError.length()) {
      payload += F(",\"first_error\":");
      appendLoomxJsonString(payload, forwardResult.firstError);
      payload += F(",\"first_error_target\":");
      appendLoomxJsonString(payload, forwardResult.firstErrorTarget);
      payload += F(",\"first_error_ip\":");
      appendLoomxJsonString(payload, forwardResult.firstErrorIp);
    }
    payload += F(",\"ack_required\":");
    payload += ackRequired ? F("true") : F("false");
    payload += '}';
    sendLoomxJson(request, payload);
  }, LOOMX_ACTION_JSON_CAPACITY);
  server.addHandler(loomxSendHandler);

  AsyncCallbackJsonWebHandler* loomxActionHandler = new AsyncCallbackJsonWebHandler(String(F("/api/loomx/action")), [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(LOOMX_ACTION_JSON_CAPACITY);
    const DeserializationError error = deserializeJson(doc, (uint8_t*)(request->_tempObject));
    if (error) {
      sendLoomxJson(request, F("{\"ok\":false,\"error\":\"invalid_json\"}"), 400);
      return;
    }

    String body;
    serializeJson(doc, body);
    uint32_t requestedDelayMs = 0;
    if (doc["execute_delay_ms"].is<uint32_t>()) requestedDelayMs = doc["execute_delay_ms"].as<uint32_t>();
    else if (doc["exec_delay_ms"].is<uint32_t>()) requestedDelayMs = doc["exec_delay_ms"].as<uint32_t>();
    const String messageType = doc["type"] | "";
    bool scheduled = requestedDelayMs >= LOOMX_SYNC_MIN_DELAY_MS && loomxActionShouldSync(messageType);
    if (!doc["execute_at_ms"].isNull() && Loomx::TimeSync::isSynced()) {
      const uint32_t localTargetMs = Loomx::TimeSync::localFor(doc["execute_at_ms"].as<uint32_t>());
      scheduled = loomxRemainingDelay(localTargetMs) >= LOOMX_SYNC_MIN_DELAY_MS && loomxActionShouldSync(messageType);
    }
    String queueError;
    const bool ok = queueLoomxAction(doc.as<JsonObject>(), body, queueError);
    if (ok && scheduled) sendLoomxJson(request, F("{\"ok\":true,\"scheduled\":true}"));
    else if (ok) sendLoomxJson(request, F("{\"ok\":true}"));
    else {
      String payload = F("{\"ok\":false,\"error\":");
      appendLoomxJsonString(payload, queueError.length() ? queueError : String(F("queue_failed")));
      payload += '}';
      sendLoomxJson(request, payload, 503);
    }
  }, LOOMX_ACTION_JSON_CAPACITY);
  server.addHandler(loomxActionHandler);
}

void initServer()
{
  //CORS compatiblity
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), "*");
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Methods"), "*");
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), "*");
  createLoomxApiHandlers();

#ifdef WLED_ENABLE_WEBSOCKETS
  #ifndef WLED_DISABLE_2D 
  server.on(F("/liveview2D"), HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_HTML), PAGE_liveviewws2D, PAGE_liveviewws2D_length);
  });
  #endif
#endif
  server.on(F("/liveview"), HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_HTML), PAGE_liveview, PAGE_liveview_length);
  });

  server.on(_common_js, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_common_js), 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), JS_common, JS_common_length);
  });

  server.on(_iro_js, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_iro_js), 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), JS_iro, JS_iro_length);
  });

  server.on(_omggif_js, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_omggif_js), 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), JS_omggif, JS_omggif_length);
  });

  //settings page
  server.on(F("/settings"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });

  // "/settings/settings.js&p=x" request also handled by serveSettings()
  static const char _style_css[] PROGMEM = "/style.css";
  server.on(_style_css, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_style_css), 200, FPSTR(CONTENT_TYPE_CSS), PAGE_settingsCss, PAGE_settingsCss_length);
  });

  static const char _favicon_ico[] PROGMEM = "/favicon.ico";
  server.on(_favicon_ico, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_favicon_ico), 200, F("image/x-icon"), favicon, favicon_length, false);
  });

  static const char _skin_css[] PROGMEM = "/skin.css";
  server.on(_skin_css, HTTP_GET, [](AsyncWebServerRequest *request) {
    if (handleFileRead(request, FPSTR(_skin_css))) return;
    AsyncWebServerResponse *response = request->beginResponse(200, FPSTR(CONTENT_TYPE_CSS));
    request->send(response);
  });

  server.on(F("/welcome"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });

  server.on(F("/reset"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 200, FPSTR(s_rebooting), F("Please wait ~10 seconds."), 131);
    doReboot = true;
  });

  server.on(F("/settings"), HTTP_POST, [](AsyncWebServerRequest *request){
    serveSettings(request, true);
  });

  const static char _json[] PROGMEM = "/json";
  server.on(FPSTR(_json), HTTP_GET, [](AsyncWebServerRequest *request){
    serveJson(request);
  });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(FPSTR(_json), [](AsyncWebServerRequest *request) {
    bool verboseResponse = false;
    bool isConfig = false;

    if (!requestJSONBufferLock(JSON_LOCK_SERVER)) {
      request->deferResponse();
      return;
    }

    DeserializationError error = deserializeJson(*pDoc, (uint8_t*)(request->_tempObject));
    JsonObject root = pDoc->as<JsonObject>();
    if (error || root.isNull()) {
      releaseJSONBufferLock();
      serveJsonError(request, 400, ERR_JSON);
      return;
    }
    if (root.containsKey("pin")) checkSettingsPIN(root["pin"].as<const char*>());

    const String& url = request->url();
    isConfig = url.indexOf(F("cfg")) > -1;
    if (!isConfig) {
      /*
      #ifdef WLED_DEBUG
        DEBUG_PRINTLN(F("Serialized HTTP"));
        serializeJson(root,Serial);
        DEBUG_PRINTLN();
      #endif
      */
      verboseResponse = deserializeState(root);
    } else {
      if (!correctPIN && strlen(settingsPIN)>0) {
        releaseJSONBufferLock();
        serveJsonError(request, 401, ERR_DENIED);
        return;
      }
      verboseResponse = deserializeConfig(root); //use verboseResponse to determine whether cfg change should be saved immediately
    }
    releaseJSONBufferLock();

    if (verboseResponse) {
      if (!isConfig) {
        lastInterfaceUpdate = millis(); // prevent WS update until cooldown
        interfaceUpdateCallMode = CALL_MODE_WS_SEND; // override call mode & schedule WS update
        #ifndef WLED_DISABLE_MQTT
        // publish state to MQTT as requested in wled#4643 even if only WS response selected
        publishMqtt();
        #endif
        serveJson(request);
        return; //if JSON contains "v"
      } else {
        configNeedsWrite = true; //Save new settings to FS
      }
    }
    request->send(200, CONTENT_TYPE_JSON, F("{\"success\":true}"));
  }, JSON_BUFFER_SIZE);
  server.addHandler(handler);

  server.on(F("/version"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, FPSTR(CONTENT_TYPE_PLAIN), (String)VERSION);
  });

  server.on(F("/uptime"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, FPSTR(CONTENT_TYPE_PLAIN), (String)millis());
  });

  server.on(F("/freeheap"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, FPSTR(CONTENT_TYPE_PLAIN), (String)getFreeHeapSize());
  });

#ifdef WLED_ENABLE_USERMOD_PAGE
  server.on("/u", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, "", 200, FPSTR(CONTENT_TYPE_HTML), PAGE_usermod, PAGE_usermod_length);
  });
#endif

  server.on(F("/teapot"), HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 418, F("418. I'm a teapot."), F("(Tangible Embedded Advanced Project Of Twinkling)"), 254);
  });

  server.on(F("/upload"), HTTP_POST, [](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
                      size_t len, bool isFinal) {handleUpload(request, filename, index, data, len, isFinal);}
  );

  createEditHandler(); // initialize "/edit" handler, access is protected by "correctPIN"

  static const char _update[] PROGMEM = "/update";
#ifndef WLED_DISABLE_OTA
  //init ota page
  server.on(_update, HTTP_GET, [](AsyncWebServerRequest *request){
    if (otaLock) {
      serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254);
    } else
      serveSettings(request); // checks for "upd" in URL and handles PIN
  });

  server.on(_update, HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->_tempObject) {
      auto ota_result = getOTAResult(request);
      if (ota_result.first == OTAResultStatus::TryAgain) {
        request->deferResponse();
      } else if (ota_result.first == OTAResultStatus::Ready) {
        if (ota_result.second.length() > 0) {
          serveMessage(request, 500, F("Update failed!"), ota_result.second, 254);
        } else {
          serveMessage(request, 200, F("Update successful!"), FPSTR(s_rebooting), 131);
        }
      }
    } else {
      // No context structure - something's gone horribly wrong
      serveMessage(request, 500, F("Update failed!"), F("Internal server fault"), 254);
    }
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal){
    if (index == 0) { 
      // Allocate the context structure
      if (!initOTA(request)) {
        return; // Error will be dealt with after upload in response handler, above
      }

      // Privilege checks
      IPAddress client  = request->client()->remoteIP();
      if (((otaSameSubnet && !inSameSubnet(client)) && !strlen(settingsPIN)) || (!otaSameSubnet && !inLocalSubnet(client))) {        
        DEBUG_PRINTLN(F("Attempted OTA update from different/non-local subnet!"));
        serveMessage(request, 401, FPSTR(s_accessdenied), F("Client is not on local subnet."), 254);
        setOTAReplied(request);
        return;
      }
      if (!correctPIN) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_cfg), 254);
        setOTAReplied(request);
        return;
      };
      if (otaLock) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254);
        setOTAReplied(request);
        return;
      }      
    }

    handleOTAData(request, index, data, len, isFinal);
  });
#else
  const auto notSupported = [](AsyncWebServerRequest *request){
    serveMessage(request, 501, FPSTR(s_notimplemented), F("This build does not support OTA update."), 254);
  };
  server.on(_update, HTTP_GET, notSupported);
  server.on(_update, HTTP_POST, notSupported, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal){});
#endif

#if defined(ARDUINO_ARCH_ESP32) && !defined(WLED_DISABLE_OTA)
  // ESP32 bootloader update endpoint
  server.on(F("/updatebootloader"), HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->_tempObject) {
      auto bootloader_result = getBootloaderOTAResult(request);
      if (bootloader_result.first) {
        if (bootloader_result.second.length() > 0) {
          serveMessage(request, 500, F("Bootloader update failed!"), bootloader_result.second, 254);
        } else {
          serveMessage(request, 200, F("Bootloader updated successfully!"), FPSTR(s_rebooting), 131);
        }
      }
    } else {
      // No context structure - something's gone horribly wrong
      serveMessage(request, 500, F("Bootloader update failed!"), F("Internal server fault"), 254);
    }
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool isFinal){
    if (index == 0) {
      // Privilege checks
      IPAddress client = request->client()->remoteIP();
      if (((otaSameSubnet && !inSameSubnet(client)) && !strlen(settingsPIN)) || (!otaSameSubnet && !inLocalSubnet(client))) {
        DEBUG_PRINTLN(F("Attempted bootloader update from different/non-local subnet!"));
        serveMessage(request, 401, FPSTR(s_accessdenied), F("Client is not on local subnet."), 254);
        setBootloaderOTAReplied(request);
        return;
      }
      if (!correctPIN) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_cfg), 254);
        setBootloaderOTAReplied(request);
        return;
      }
      if (otaLock) {
        serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254);
        setBootloaderOTAReplied(request);
        return;
      }

      // Allocate the context structure
      if (!initBootloaderOTA(request)) {
        return; // Error will be dealt with after upload in response handler, above
      }
    }

    handleBootloaderOTAData(request, index, data, len, isFinal);
  });
#endif

#ifdef WLED_ENABLE_DMX
  server.on(F("/dmxmap"), HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, FPSTR(CONTENT_TYPE_HTML), PAGE_dmxmap, dmxProcessor);
  });
#endif

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (captivePortal(request)) return;
    serveLoomxIndex(request);
  });

  server.on(F("/index.htm"), HTTP_GET, [](AsyncWebServerRequest *request) {
    serveLoomxIndex(request);
  });

#ifndef WLED_DISABLE_2D
  #ifdef WLED_ENABLE_PIXART
  static const char _pixart_htm[] PROGMEM = "/pixart.htm";
  server.on(_pixart_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_pixart_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_pixart, PAGE_pixart_length);
  });
  #endif

  #ifdef WLED_ENABLE_PXMAGIC
  static const char _pxmagic_htm[] PROGMEM = "/pxmagic.htm";
  server.on(_pxmagic_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_pxmagic_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_pxmagic, PAGE_pxmagic_length);
  });
  #endif

  #ifndef WLED_DISABLE_PIXELFORGE
  static const char _pixelforge_htm[] PROGMEM = "/pixelforge.htm";
  server.on(_pixelforge_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_pixelforge_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_pixelforge, PAGE_pixelforge_length);
  });
  #else
  server.on("/pixelforge.htm", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",
      F("<!DOCTYPE html><html><head><title>PixelForge</title></head>"
      "<style>body{background:#000;color:#fff;font-family:sans-serif;display:flex;justify-content:center;}</style>"
      "<body><h2>Sorry, PixelForge is not supported in this build.</h2></body></html>")
    );
  });
  #endif
#endif

  static const char _cpal_htm[] PROGMEM = "/cpal.htm";
  server.on(_cpal_htm, HTTP_GET, [](AsyncWebServerRequest *request) {
    handleStaticContent(request, FPSTR(_cpal_htm), 200, FPSTR(CONTENT_TYPE_HTML), PAGE_cpal, PAGE_cpal_length);
  });

#ifdef WLED_ENABLE_WEBSOCKETS
  server.addHandler(&ws);
#endif

  //called when the url is not defined here, ajax-in; get-settings
  server.onNotFound([](AsyncWebServerRequest *request){
    DEBUG_PRINTF_P(PSTR("Not-Found HTTP call: %s\n"), request->url().c_str());
    if (captivePortal(request)) return;

    //make API CORS compatible
    if (request->method() == HTTP_OPTIONS)
    {
      AsyncWebServerResponse *response = request->beginResponse(200);
      response->addHeader(F("Access-Control-Max-Age"), F("7200"));
      request->send(response);
      return;
    }

    if(handleSet(request, request->url())) return;
    #ifndef WLED_DISABLE_ALEXA
    if(espalexa.handleAlexaApiCall(request)) return;
    #endif
    handleStaticContent(request, request->url(), 404, FPSTR(CONTENT_TYPE_HTML), PAGE_404, PAGE_404_length);
  });
}


void serveMessage(AsyncWebServerRequest* request, uint16_t code, const String& headl, const String& subl, byte optionT)
{
  messageHead = headl;
  messageSub = subl;
  optionType = optionT;

  request->send_P(code, FPSTR(CONTENT_TYPE_HTML), PAGE_msg, msgProcessor);
}


void serveJsonError(AsyncWebServerRequest* request, uint16_t code, uint16_t error)
{
    AsyncJsonResponse *response = new AsyncJsonResponse(64);
    if (error < ERR_NOT_IMPL) response->addHeader(F("Retry-After"), F("1"));
    response->setContentType(CONTENT_TYPE_JSON);
    response->setCode(code);
    JsonObject obj = response->getRoot();
    obj[F("error")] = error;
    response->setLength();
    request->send(response);
}


void serveSettingsJS(AsyncWebServerRequest* request)
{
  if (request->url().indexOf(FPSTR(_common_js)) > 0) {
    handleStaticContent(request, FPSTR(_common_js), 200, FPSTR(CONTENT_TYPE_JAVASCRIPT), JS_common, JS_common_length);
    return;
  }
  byte subPage = request->arg(F("p")).toInt();
  if (subPage > SUBPAGE_LAST) {
    request->send_P(501, FPSTR(CONTENT_TYPE_JAVASCRIPT), PSTR("alert('Settings for this request are not implemented.');"));
    return;
  }
  if (subPage > 0 && !correctPIN && strlen(settingsPIN)>0) {
    request->send_P(401, FPSTR(CONTENT_TYPE_JAVASCRIPT), PSTR("alert('PIN incorrect.');"));
    return;
  }
  
  AsyncResponseStream *response = request->beginResponseStream(FPSTR(CONTENT_TYPE_JAVASCRIPT));
  response->addHeader(FPSTR(s_cache_control), FPSTR(s_no_store));
  response->addHeader(FPSTR(s_expires), F("0"));

  response->print(F("function GetV(){var d=document;"));
  getSettingsJS(subPage, *response);
  response->print(F("}"));
  request->send(response);
}


void serveSettings(AsyncWebServerRequest* request, bool post) {
  byte subPage = 0, originalSubPage = 0;
  const String& url = request->url();

  if (url.indexOf("sett") >= 0) {
    if      (url.indexOf(F(".js"))  > 0) subPage = SUBPAGE_JS;
    else if (url.indexOf(F(".css")) > 0) subPage = SUBPAGE_CSS;
    else if (url.indexOf(F("wifi")) > 0) subPage = SUBPAGE_WIFI;
    else if (url.indexOf(F("leds")) > 0) subPage = SUBPAGE_LEDS;
    else if (url.indexOf(F("ui"))   > 0) subPage = SUBPAGE_UI;
    else if (url.indexOf(  "sync")  > 0) subPage = SUBPAGE_SYNC;
    else if (url.indexOf(  "time")  > 0) subPage = SUBPAGE_TIME;
    else if (url.indexOf(F("sec"))  > 0) subPage = SUBPAGE_SEC;
#ifdef WLED_ENABLE_DMX
    else if (url.indexOf(  "dmx")   > 0) subPage = SUBPAGE_DMX;
#endif
    else if (url.indexOf(  "um")    > 0) subPage = SUBPAGE_UM;
#ifndef WLED_DISABLE_2D
    else if (url.indexOf(  "2D")    > 0) subPage = SUBPAGE_2D;
#endif
    else if (url.indexOf(F("pins")) > 0) subPage = SUBPAGE_PINS;
    else if (url.indexOf(F("lock")) > 0) subPage = SUBPAGE_LOCK;
  }
  else if (url.indexOf("/update") >= 0) subPage = SUBPAGE_UPDATE; // update page, for PIN check
  //else if (url.indexOf("/edit")   >= 0) subPage = 10;
  else subPage = SUBPAGE_WELCOME;

  bool pinRequired = !correctPIN && strlen(settingsPIN) > 0 && (subPage > (WLED_WIFI_CONFIGURED ? SUBPAGE_MENU : SUBPAGE_WIFI) && subPage < SUBPAGE_LOCK);
  if (pinRequired) {
    originalSubPage = subPage;
    subPage = SUBPAGE_PINREQ; // require PIN
  }

  // if OTA locked or too frequent PIN entry requests fail hard
  if ((subPage == SUBPAGE_WIFI && wifiLock && otaLock) || (post && pinRequired && millis()-lastEditTime < PIN_RETRY_COOLDOWN))
  {
    serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_unlock_ota), 254); return;
  }

  if (post) { //settings/set POST request, saving
    IPAddress client = request->client()->remoteIP();
    if (!inLocalSubnet(client)) { // includes same subnet check
      serveMessage(request, 401, FPSTR(s_accessdenied), FPSTR(s_redirecting), 123);
      return;
    }
    if (subPage != SUBPAGE_WIFI || !(wifiLock && otaLock)) handleSettingsSet(request, subPage);

    char s[32];
    char s2[45] = "";

    switch (subPage) {
      case SUBPAGE_WIFI   : strcpy_P(s, PSTR("WiFi")); strcpy_P(s2, PSTR("Please connect to the new IP (if changed)")); break;
      case SUBPAGE_LEDS   : strcpy_P(s, PSTR("LED")); break;
      case SUBPAGE_UI     : strcpy_P(s, PSTR("UI")); break;
      case SUBPAGE_SYNC   : strcpy_P(s, PSTR("Sync")); break;
      case SUBPAGE_TIME   : strcpy_P(s, PSTR("Time")); break;
      case SUBPAGE_SEC    : strcpy_P(s, PSTR("Security")); if (doReboot) strcpy_P(s2, PSTR("Rebooting, please wait ~10 seconds...")); break;
#ifdef WLED_ENABLE_DMX
      case SUBPAGE_DMX    : strcpy_P(s, PSTR("DMX")); break;
#endif
      case SUBPAGE_UM     : strcpy_P(s, PSTR("Usermods")); break;
#ifndef WLED_DISABLE_2D
      case SUBPAGE_2D     : strcpy_P(s, PSTR("2D")); break;
#endif
      case SUBPAGE_PINREQ : strcpy_P(s, correctPIN ? PSTR("PIN accepted") : PSTR("PIN rejected")); break;
    }

    if (subPage != SUBPAGE_PINREQ) strcat_P(s, PSTR(" settings saved."));

    if (subPage == SUBPAGE_PINREQ && correctPIN) {
      subPage = originalSubPage; // on correct PIN load settings page the user intended
    } else {
      if (!s2[0]) strcpy_P(s2, s_redirecting);

      bool redirectAfter9s = (subPage == SUBPAGE_WIFI || ((subPage == SUBPAGE_SEC || subPage == SUBPAGE_UM) && doReboot));
      serveMessage(request, (!pinRequired ? 200 : 401), s, s2, redirectAfter9s ? 129 : (!pinRequired ? 1 : 3));
      return;
    }
  }

  int code = 200;
  String contentType = FPSTR(CONTENT_TYPE_HTML);
  const uint8_t* content;
  size_t len;

  switch (subPage) {
    case SUBPAGE_WIFI    :  content = PAGE_settings_wifi; len = PAGE_settings_wifi_length; break;
    case SUBPAGE_LEDS    :  content = PAGE_settings_leds; len = PAGE_settings_leds_length; break;
    case SUBPAGE_UI      :  content = PAGE_settings_ui;   len = PAGE_settings_ui_length;   break;
    case SUBPAGE_SYNC    :  content = PAGE_settings_sync; len = PAGE_settings_sync_length; break;
    case SUBPAGE_TIME    :  content = PAGE_settings_time; len = PAGE_settings_time_length; break;
    case SUBPAGE_SEC     :  content = PAGE_settings_sec;  len = PAGE_settings_sec_length;  break;
#ifdef WLED_ENABLE_DMX
    case SUBPAGE_DMX     :  content = PAGE_settings_dmx;  len = PAGE_settings_dmx_length;  break;
#endif
    case SUBPAGE_UM      :  content = PAGE_settings_um;   len = PAGE_settings_um_length;   break;
#ifndef WLED_DISABLE_OTA
    case SUBPAGE_UPDATE  :  content = PAGE_update;        len = PAGE_update_length;
      #ifdef ARDUINO_ARCH_ESP32
      if (request->hasArg(F("revert")) && inLocalSubnet(request->client()->remoteIP()) && Update.canRollBack()) {
        doReboot = Update.rollBack();
        if (doReboot) {
          serveMessage(request, 200, F("Reverted to previous version!"), FPSTR(s_rebooting), 133);
        } else {
          serveMessage(request, 500, F("Rollback failed!"), F("Please reboot and retry."), 254);
        }
        return;
      }
      #endif
      break;
#endif
#ifndef WLED_DISABLE_2D
    case SUBPAGE_2D      :  content = PAGE_settings_2D;   len = PAGE_settings_2D_length;   break;
#endif
    case SUBPAGE_PINS    :  content = PAGE_settings_pininfo; len = PAGE_settings_pininfo_length; break;
    case SUBPAGE_LOCK    : {
      correctPIN = !strlen(settingsPIN); // lock if a pin is set
      serveMessage(request, 200, strlen(settingsPIN) > 0 ? PSTR("Settings locked") : PSTR("No PIN set"), FPSTR(s_redirecting), 1);
      return;
    }
    case SUBPAGE_PINREQ  :  content = PAGE_settings_pin;  len = PAGE_settings_pin_length; code = 401;                 break;
    case SUBPAGE_CSS     :  content = PAGE_settingsCss;   len = PAGE_settingsCss_length;  contentType = FPSTR(CONTENT_TYPE_CSS); break;
    case SUBPAGE_JS      :  serveSettingsJS(request); return;
    case SUBPAGE_WELCOME :  content = PAGE_welcome;       len = PAGE_welcome_length;       break;
    default:                content = PAGE_settings;      len = PAGE_settings_length;      break;
  }
  handleStaticContent(request, "", code, contentType, content, len);
}
