#include "Platform.h"
#include <esp_partition.h>
namespace shap {
DynamicJsonDocument config(MAX_CONFIG_BYTES);
Preferences prefs;
static bool validConfig(JsonDocument& doc) {
  if (doc["schema"] != 1 || doc["board"] != BOARD || !doc["devices"].is<JsonArray>() ||
      !doc["automations"].is<JsonArray>() || !doc["scenes"].is<JsonArray>() ||
      !doc["schedules"].is<JsonArray>() || !doc["dashboard"]["widgets"].is<JsonArray>()) return false;
  JsonArrayConst devices=doc["devices"].as<JsonArrayConst>();
  if (devices.size()>MAX_DEVICES || doc["automations"].as<JsonArrayConst>().size()>MAX_RULES ||
      doc["scenes"].as<JsonArrayConst>().size()>MAX_SCENES ||
      doc["schedules"].as<JsonArrayConst>().size()>MAX_SCHEDULES ||
      doc["dashboard"]["widgets"].as<JsonArrayConst>().size()>MAX_WIDGETS) return false;
  String error;
  for (size_t i=0;i<devices.size();i++) {
    String id=devices[i]["id"] | "";
    if (id.isEmpty() || id.length()>16 || !validateDevice(devices[i],id,error)) return false;
    for (size_t j=i+1;j<devices.size();j++) {
      if (devices[j]["id"]==id || (devices[i]["type"]!="virtual" && devices[j]["type"]!="virtual" && devices[i]["pin"]==devices[j]["pin"])) return false;
    }
  }
  return true;
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
// Autoformat only a genuinely erased/virgin partition; never wipe a damaged existing config.
static bool virginFilesystem() {
  const esp_partition_t* partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
  if (!partition) return false;
  uint8_t bytes[256];
  for (size_t at=0;at<partition->size;at+=sizeof(bytes)) {
    size_t count=min(sizeof(bytes),partition->size-at);
    if (esp_partition_read(partition,at,bytes,count)!=ESP_OK) return false;
    for (size_t i=0;i<count;i++) if (bytes[i]!=0xff) return false;
  }
  return true;
}
bool loadConfig() {
  if (!LittleFS.begin(false)) {
    if (!virginFilesystem() || !LittleFS.begin(true)) {
      Serial.println("LittleFS mount failed; existing data NOT formatted. Web assets are still embedded in firmware.");
      ensureConfig(); return false;
    }
    Serial.println("Blank config partition initialized; embedded web assets need no filesystem upload");
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
