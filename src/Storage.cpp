#include "Platform.h"
namespace shap {
DynamicJsonDocument config(MAX_CONFIG_BYTES);
Preferences prefs;
static bool validConfig(JsonDocument& doc) {
  return doc["schema"] == 1 && doc["board"] == BOARD && doc["devices"].is<JsonArray>() && doc["automations"].is<JsonArray>() && doc["scenes"].is<JsonArray>() && doc["schedules"].is<JsonArray>() && doc["dashboard"]["widgets"].is<JsonArray>();
}
void ensureConfig() {
  config.clear();
  config["schema"] = 1; config["board"] = BOARD;
  config["name"] = "My Smart Home"; config["timezone"] = "UTC0";
  config.createNestedArray("devices"); config.createNestedArray("automations");
  config.createNestedArray("scenes"); config.createNestedArray("schedules");
  JsonObject dashboard = config.createNestedObject("dashboard");
  dashboard["name"] = "Home"; dashboard.createNestedArray("widgets");
}
bool loadConfig() {
  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS mount failed; flash assets/config unavailable. Upload filesystem image.");
    ensureConfig(); return false;
  }
  for (auto path : {"/config.json", "/config.bak"}) {
    File f = LittleFS.open(path, "r");
    if (!f || f.size() > MAX_CONFIG_BYTES || f.size() < 16) { if (f) f.close(); continue; }
    config.clear(); DeserializationError e = deserializeJson(config, f); f.close();
    if (!e && validConfig(config)) {
      if (String(path)=="/config.bak") {
        // Recover the last valid version without discarding it before the replacement exists.
        LittleFS.remove("/config.json"); LittleFS.rename("/config.bak","/config.json");
      }
      return true;
    }
  }
  ensureConfig(); return false;
}
bool saveConfig() {
  if (config.overflowed() || measureJson(config) > MAX_CONFIG_BYTES - 512) return false;
  File f = LittleFS.open("/config.tmp", "w"); if (!f) return false;
  size_t expected = measureJson(config), written = serializeJson(config, f); f.flush(); f.close();
  if (written != expected) { LittleFS.remove("/config.tmp"); return false; }
  // Retain the last good version until the replacement has been written completely.
  LittleFS.remove("/config.bak");
  if (LittleFS.exists("/config.json")) LittleFS.rename("/config.json", "/config.bak");
  if (LittleFS.rename("/config.tmp", "/config.json")) return true;
  LittleFS.rename("/config.bak", "/config.json"); return false;
}
}
