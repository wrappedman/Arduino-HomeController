#include "Platform.h"
namespace shap {
struct Pending { String id, action; unsigned long due; };
static Pending queue[16]; static uint8_t head=0, tail=0;
static unsigned long ruleRan[MAX_RULES] = {};
static long lastScheduleMinute=-1;
static bool enqueue(const String& id, const String& action, uint32_t delayMs) {
  uint8_t next=(tail+1)%16; if (next==head) { recordEvent("Automation queue full; action dropped","error"); return false; }
  queue[tail]={id,action,millis()+delayMs}; tail=next; return true;
}
static bool runScene(const String& id) {
  for (JsonObject s : config["scenes"].as<JsonArray>()) if (s["id"]==id) {
    for (JsonObject a : s["actions"].as<JsonArray>()) enqueue(a["deviceId"] | "",a["command"] | "",0);
    recordEvent("Scene " + s["name"].as<String>() + " queued","scene"); return true;
  }
  return false;
}
bool activateScene(const String& id) { return runScene(id); }
static bool conditionsMatch(JsonObject rule) {
  String id=rule["conditionDeviceId"] | ""; if (id.isEmpty()) return true;
  for (JsonObject d : config["devices"].as<JsonArray>()) if (d["id"]==id && (d["enabled"] | true)) {
    float v=d["value"] | 0.0f, target=rule["conditionValue"] | 0.0f; String op=rule["conditionOp"] | "equals";
    return op=="above" ? v>target : op=="below" ? v<target : abs(v-target)<0.5f;
  }
  return false;
}
void deviceChanged(const String& id, float value) {
  int i=0;
  for (JsonObject r : config["automations"].as<JsonArray>()) {
    if (i>=MAX_RULES) break;
    if ((r["enabled"] | true) && r["triggerDeviceId"]==id && millis()-ruleRan[i]>1000) {
      String op=r["triggerOp"] | "equals"; float target=r["triggerValue"] | 1.0f;
      bool match=op=="above" ? value>target : op=="below" ? value<target : abs(value-target)<0.5f;
      if (match && conditionsMatch(r)) { ruleRan[i]=millis(); enqueue(r["actionDeviceId"] | "",r["action"] | "on", min(3600000UL,(uint32_t)(r["delaySeconds"] | 0)*1000UL)); recordEvent("Rule triggered: " + r["name"].as<String>(),"automation"); }
    }
    i++;
  }
}
void tickAutomation() {
  // Rotate not-yet-due items so a delayed action cannot starve a later immediate one.
  // Bounded scan/execution also prevents chains of rules from running recursively.
  int scanned=0, executed=0;
  while (head!=tail && scanned<15 && executed<4) {
    Pending p=queue[head]; head=(head+1)%16; scanned++;
    if ((long)(millis()-p.due)<0) { queue[tail]=p; tail=(tail+1)%16; continue; }
    executed++;
    if (p.action=="scene") runScene(p.id);
    else if (!commandDevice(p.id,p.action,"automation")) recordEvent("Automation action skipped: device unavailable","error");
  }
  if (!clockValid) return;
  time_t now=time(nullptr); tm local; localtime_r(&now,&local); long minute=now/60;
  if (minute==lastScheduleMinute) return; lastScheduleMinute=minute;
  char timeBuf[6]; snprintf(timeBuf,sizeof(timeBuf),"%02d:%02d",local.tm_hour,local.tm_min);
  for (JsonObject s : config["schedules"].as<JsonArray>()) {
    if (!(s["enabled"] | true) || s["time"]!=timeBuf) continue;
    // days bitmask: Sunday=1, Monday=2 ... Saturday=64
    if (((s["days"] | 127) & (1 << local.tm_wday))==0) continue;
    enqueue(s["targetId"] | "",s["action"] | "on",0);
    recordEvent("Schedule triggered: " + s["name"].as<String>(),"schedule");
  }
}
}
