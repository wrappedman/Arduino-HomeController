#include "Platform.h"
#include <cmath>
namespace shap {
DynamicJsonDocument events(8192);
static unsigned long onSince[MAX_DEVICES] = {};
static bool validPin(int pin, const String& type) {
  // Conservative DevKitC-1 N16R8 allowlist. 0/19/20/26-37/43-46 are excluded.
  bool safe = ((pin >= 1 && pin <= 18) && pin != 8 && pin != 9 && pin != 10 && pin != 11 && pin != 12 && pin != 13) || pin == 21 || (pin >= 38 && pin <= 42) || pin == 47;
  if (!safe) return false;
  if (type == "analog") return pin >= 1 && pin <= 10 && pin != 8 && pin != 9 && pin != 10 && pin != 11 && pin != 12 && pin != 13; // ADC1 only; avoid ADC2/Wi-Fi conflicts.
  return type == "relay" || type == "digital";
}
bool validateDevice(JsonVariantConst device, const String& ignoreId, String& error) {
  String type = device["type"] | ""; String name = device["name"] | "";
  if (name.length()<1 || name.length()>48) { error="Name must be 1–48 characters"; return false; }
  if (type != "relay" && type != "digital" && type != "analog" && type != "virtual") { error="Unsupported driver (relay, digital, analog, virtual)"; return false; }
  if (type != "virtual") {
    int pin = device["pin"] | -1;
    if (!validPin(pin,type)) { error="GPIO unavailable or incompatible with this driver/board"; return false; }
    for (JsonVariantConst d : config["devices"].as<JsonArrayConst>()) {
      if (d["id"].as<String>() != ignoreId && d["type"] != "virtual" && d["pin"].as<int>() == pin) { error="GPIO already assigned"; return false; }
    }
  }
  if (type == "relay" && device["maxOnSeconds"].as<int>() < 0) { error="Invalid activation limit"; return false; }
  return true;
}
void recordEvent(const String& message, const String& category) {
  JsonArray list = events.as<JsonArray>();
  if (list.size() >= 40 || events.memoryUsage()>7000) {
    list.remove(0);
    // ArduinoJson uses a monotonic pool; remove() alone does NOT release memory.
    if (!events.garbageCollect()) events.to<JsonArray>(); // clear history if memory is exhausted
    list=events.as<JsonArray>();
  }
  JsonObject e = list.createNestedObject(); e["message"] = message.substring(0,120); e["category"] = category.substring(0,20);
  e["at"] = clockValid ? (long)time(nullptr) : 0; e["uptime"] = millis()/1000;
  String payload; DynamicJsonDocument d(320); d["type"]="event"; d["data"]=e; serializeJson(d,payload); broadcastMessage(payload);
}
// Drives one device's GPIO into its safe idle state. Relays latch their OFF level
// before the output driver is enabled so the pin never energises a load at boot.
static void configurePin(JsonObject d) {
  String type=d["type"] | ""; int pin=d["pin"] | -1;
  if (type=="relay" && validPin(pin,type)) {
    pinMode(pin, INPUT);
    digitalWrite(pin, (d["activeLow"] | false) ? HIGH : LOW);
    pinMode(pin, OUTPUT);
  } else if (type=="digital" && validPin(pin,type)) {
    pinMode(pin, (d["pullup"] | false) ? INPUT_PULLUP : INPUT);
  } else if (type=="analog" && validPin(pin,type)) { pinMode(pin, INPUT); }
}
void initDevice(JsonObject d) { configurePin(d); }
void releaseDevice(int pin) { if (validPin(pin, "relay")) pinMode(pin, INPUT); }
// Re-apply pin modes after a configuration rollback without discarding live values.
void syncHardware() {
  for (JsonObject d : config["devices"].as<JsonArray>()) configurePin(d);
}
void initHardware() {
  if (!events.is<JsonArray>()) events.to<JsonArray>();
  int index=0;
  for (JsonObject d : config["devices"].as<JsonArray>()) {
    d["value"] = 0;
    configurePin(d);
    if (index < MAX_DEVICES) onSince[index++] = 0;
  }
  recordEvent("GPIO initialized; relay outputs set to safe OFF state");
}
void applyOutputs() {
  int i=0;
  for (JsonObject d : config["devices"].as<JsonArray>()) {
    String type=d["type"] | ""; int pin=d["pin"] | -1;
    if (type=="relay" && validPin(pin,type)) {
      bool on=d["value"].as<float>()>0;
      digitalWrite(pin, on == (d["activeLow"] | false) ? LOW : HIGH);
      if (i < MAX_DEVICES) {
        // Re-asserting an output keeps its original switch-on time so the
        // maximum-ON safety timer is never restarted by an unrelated save.
        if (!on) onSince[i] = 0;
        else if (!onSince[i]) onSince[i] = millis();
      }
    }
    i++;
  }
}
static JsonObject findDevice(const String& id) {
  for (JsonObject d : config["devices"].as<JsonArray>()) if (d["id"] == id) return d;
  return JsonObject();
}
bool commandDevice(const String& id, const String& command, const String& source) {
  JsonObject d=findDevice(id); if (d.isNull() || (d["enabled"] | true)==false) return false;
  String type=d["type"] | ""; if (type!="relay" && type!="virtual") return false;
  if (type=="relay" && !validPin(d["pin"] | -1,type)) return false;
  float old=d["value"] | 0.0f;
  float next=command=="on" ? 1 : command=="off" ? 0 : command=="toggle" ? (old>0?0:1) : -1;
  if (next<0) return false;
  if (next && type=="relay") {
    String group=d["interlock"] | "";
    if (group.length()) for (JsonObject other : config["devices"].as<JsonArray>()) {
      if (other["id"] != id && other["type"]=="relay" && other["interlock"]==group && other["value"].as<float>()>0) commandDevice(other["id"].as<String>(),"off","interlock");
    }
  }
  d["value"]=next; int pin=d["pin"] | -1;
  if (type=="relay" && validPin(pin,type)) {
    digitalWrite(pin, next == (d["activeLow"] | false) ? LOW : HIGH);
    int idx=0; for (JsonObject it : config["devices"].as<JsonArray>()) { if (it["id"]==id) { if (idx<MAX_DEVICES) { if (next!=old) onSince[idx]=next ? millis() : 0; else if (next && !onSince[idx]) onSince[idx]=millis(); } break; } idx++; }
  }
  if (next!=old) { recordEvent(d["name"].as<String>() + (next?" turned on":" turned off") + " by " + source,"device"); broadcastState(id,next); deviceChanged(id,next); }
  return true;
}
void tickHardware() {
  static unsigned long last=0;
  if (millis()-last<1000) return; last=millis(); int idx=0;
  static unsigned long lastHistorySample=0; bool collect=millis()-lastHistorySample>=30000;
  for (JsonObject d : config["devices"].as<JsonArray>()) {
    String type=d["type"] | ""; int pin=d["pin"] | -1;
    if (type=="relay" && idx<MAX_DEVICES && d["value"].as<float>()>0 && (d["maxOnSeconds"] | 0)>0 && millis()-onSince[idx] > (uint32_t)d["maxOnSeconds"].as<int>()*1000UL) commandDevice(d["id"].as<String>(),"off","safety timer");
    if ((type=="digital" || type=="analog") && validPin(pin,type)) {
      float previous=d["value"] | 0.0f, value;
      if (type=="analog") {
        // Invert the raw 0–100 reading, then calibrate: invert is a sensor
        // characteristic, not a property of the scaled engineering value.
        value=analogRead(pin)/4095.0f*100.0f;
        if (d["invert"] | false) value=100.0f-value;
        value=value*(d["scale"] | 1.0f)+(d["offset"] | 0.0f);
      } else {
        value=digitalRead(pin);
        if (d["invert"] | false) value=1-value;
      }
      d["value"]=value; d["lastSeen"] = (long)time(nullptr);
      if (collect) sampleHistory(d["id"].as<String>(),value);
      if (abs(previous-value) >= (type=="analog" ? 1.0f : 0.5f)) { broadcastState(d["id"].as<String>(),value); deviceChanged(d["id"].as<String>(),value); }
    }
    idx++;
  }
  if (collect) lastHistorySample=millis();
}
}
