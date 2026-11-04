#include "Platform.h"
#include <mbedtls/sha256.h>
#include <cstdio>
namespace shap {
WebServer web(80); WebSocketsServer socket(81);
static bool socketAuth[16]={};
static String ticket; static unsigned long ticketAt=0;
static bool updateOk=false, hashStarted=false; static String updateExpected;
static mbedtls_sha256_context updateHash;
static bool sameOrigin() { String o=web.header("Origin"); return o.isEmpty() || o=="http://"+web.hostHeader(); }
void jsonResponse(int code, JsonVariantConst value) {
  String out; serializeJson(value,out); web.sendHeader("Cache-Control","no-store"); web.send(code,"application/json",out);
}
void errorResponse(int code, const String& name, const String& message) {
  DynamicJsonDocument d(320); d["error"]["code"]=name; d["error"]["message"]=message; jsonResponse(code,d.as<JsonVariantConst>());
}
bool readJson(DynamicJsonDocument& out, size_t limit) {
  String body=web.arg("plain");
  if (body.length()==0 || body.length()>limit) { errorResponse(413,"invalid_size","JSON request empty or too large"); return false; }
  if (deserializeJson(out,body) || !out.is<JsonObject>()) { errorResponse(400,"invalid_json","Expected a JSON object"); return false; }
  return true;
}
static bool auth(bool write=false) {
  if (setupMode) { errorResponse(403,"setup_required","Complete setup first"); return false; }
  if (!authorized(write)) { errorResponse(401,"unauthorized","Login or CSRF token required"); return false; }
  return true;
}
void broadcastMessage(const String& payload) {
  for (uint8_t i=0;i<16;i++) if (socketAuth[i]) socket.sendTXT(i,payload);
}
void broadcastState(const String& id, float value) {
  DynamicJsonDocument d(192); d["type"]="device"; d["id"]=id; d["value"]=value;
  String out; serializeJson(d,out); broadcastMessage(out);
}
static void onSocket(uint8_t n, WStype_t type, uint8_t* data, size_t len) {
  if (n>=16) { socket.disconnect(n); return; }
  if (type==WStype_DISCONNECTED || type==WStype_CONNECTED) { socketAuth[n]=false; return; }
  if (type!=WStype_TEXT || len>128) { socket.disconnect(n); return; }
  String text; text.reserve(len); for (size_t i=0;i<len;i++) text+=(char)data[i];
  if (!socketAuth[n] && ticket.length() && text=="auth:"+ticket && millis()-ticketAt<30000) {
    socketAuth[n]=true; ticket=""; socket.sendTXT(n,"{\"type\":\"connected\"}");
  } else socket.disconnect(n); // WebSocket is broadcast-only; commands go through authenticated REST.
}
static JsonArray collection(const String& key) { return config[key].as<JsonArray>(); }
static String collectionKey(const String& path) {
  for (const char* key : {"devices","automations","scenes","schedules"}) if (path==String("/api/")+key || path.startsWith(String("/api/")+key+"/")) return key;
  return "";
}
static String deviceType(const String& id) {
  for (JsonObject d : config["devices"].as<JsonArray>()) if (d["id"]==id) return d["type"].as<String>();
  return "";
}
static bool validTarget(const String& id, const String& action) {
  if (action=="scene") {
    for (JsonObject s : config["scenes"].as<JsonArray>()) if (s["id"]==id) return true;
    return false;
  }
  String type=deviceType(id); return type=="relay" || type=="virtual";
}
static bool validIdentifier(const String& id) {
  if (id.isEmpty() || id.length()>16) return false;
  for (size_t i=0;i<id.length();i++) if (!isAlphaNumeric(id[i]) && id[i]!='_') return false;
  return true;
}
static bool validateRecord(const String& key, JsonVariantConst body, const String& id, String& issue) {
  String name=body["name"] | "";
  if (name.isEmpty() || name.length()>48) { issue="Name must be 1–48 characters"; return false; }
  if (key=="devices") return validateDevice(body,id,issue);
  if (key=="automations") {
    String op=body["triggerOp"] | ""; String action=body["action"] | "";
    if (op!="equals" && op!="above" && op!="below") { issue="Invalid trigger operator"; return false; }
    if (action!="on" && action!="off" && action!="toggle" && action!="scene") { issue="Invalid action"; return false; }
    if ((body["delaySeconds"] | 0)<0 || (body["delaySeconds"] | 0)>3600) { issue="Delay must be 0–3600 seconds"; return false; }
    String trigger=body["triggerDeviceId"] | "", target=body["actionDeviceId"] | "", condition=body["conditionDeviceId"] | "";
    if (deviceType(trigger).isEmpty() || !validTarget(target,action) || (condition.length() && deviceType(condition).isEmpty())) { issue="Rule references an unavailable or incompatible device/scene"; return false; }
  }
  if (key=="scenes") {
    if (!body["actions"].is<JsonArrayConst>() || body["actions"].as<JsonArrayConst>().size()>16) { issue="Scene actions required (max 16)"; return false; }
    for (JsonObjectConst a : body["actions"].as<JsonArrayConst>()) if ((a["command"]!="on" && a["command"]!="off" && a["command"]!="toggle") || !validTarget(a["deviceId"] | "",a["command"] | "")) { issue="Invalid scene action or device"; return false; }
  }
  if (key=="schedules") {
    String time=body["time"] | ""; int h=-1,m=-1;
    if (time.length()!=5 || sscanf(time.c_str(),"%2d:%2d",&h,&m)!=2 || h<0 || h>23 || m<0 || m>59 || time[2]!=':') { issue="Time must be HH:MM"; return false; }
    String action=body["action"] | ""; if (action!="on" && action!="off" && action!="toggle" && action!="scene") { issue="Invalid schedule action"; return false; }
    if (!validTarget(body["targetId"] | "",action)) { issue="Schedule target is unavailable or incompatible"; return false; }
    if ((body["days"] | 0)<1 || (body["days"] | 0)>127) { issue="Select at least one weekday"; return false; }
  }
  return true;
}
static void handleCollection(const String& path) {
  String key=collectionKey(path); if (key.isEmpty()) { errorResponse(404,"not_found","Route not found"); return; }
  if (!auth(web.method()!=HTTP_GET)) return;
  String base="/api/"+key;
  String rest=path.length()>base.length() ? path.substring(base.length()+1) : "";
  if (rest.indexOf('/')>=0) {
    if (key=="devices" && rest.endsWith("/command") && web.method()==HTTP_POST) {
      String id=rest.substring(0,rest.length()-8); DynamicJsonDocument b(256); if (!readJson(b,256)) return;
      if (!commandDevice(id,b["command"] | "","web")) { errorResponse(400,"invalid_command","Device or command unavailable"); return; }
      web.send(200,"application/json","{\"ok\":true}"); return;
    }
    errorResponse(404,"not_found","Route not found"); return;
  }
  JsonArray arr=collection(key);
  if (web.method()==HTTP_GET) {
    if (rest.isEmpty()) { jsonResponse(200,arr); return; }
    for (JsonObject o : arr) if (o["id"]==rest) { jsonResponse(200,o); return; }
    errorResponse(404,"not_found","Record not found"); return;
  }
  if (web.method()==HTTP_POST || web.method()==HTTP_PUT) {
    if ((web.method()==HTTP_POST && !rest.isEmpty()) || (web.method()==HTTP_PUT && rest.isEmpty())) { errorResponse(405,"method_not_allowed","Invalid method for path"); return; }
    DynamicJsonDocument body(8192); if (!readJson(body,8192)) return;
    int maximum=key=="devices" ? MAX_DEVICES : key=="automations" ? MAX_RULES : key=="scenes" ? MAX_SCENES : MAX_SCHEDULES;
    if (web.method()==HTTP_POST && arr.size()>=maximum) { errorResponse(409,"limit_reached","Collection is full"); return; }
    JsonObject existing;
    if (web.method()==HTTP_PUT) {
      for (JsonObject o : arr) if (o["id"]==rest) existing=o;
      if (existing.isNull()) { errorResponse(404,"not_found","Record not found"); return; }
    }
    String issue; if (!validateRecord(key,body.as<JsonVariantConst>(),rest,issue)) { errorResponse(422,"invalid_record",issue); return; }
    DynamicJsonDocument previous(MAX_CONFIG_BYTES); previous.set(config);
    if (key=="devices" && !existing.isNull() && existing["type"]=="relay") { int oldPin=existing["pin"] | -1; commandDevice(rest,"off","configuration"); if (oldPin>=0) pinMode(oldPin,INPUT); }
    String id=rest.isEmpty() ? randomHex(6) : rest;
    if (existing.isNull()) existing=arr.createNestedObject();
    existing.set(body.as<JsonVariantConst>()); existing["id"]=id;
    if (key=="devices") { existing["value"]=0; initHardware(); applyOutputs(); }
    if (!saveConfig()) { config.set(previous); if (key=="devices") { initHardware(); applyOutputs(); } errorResponse(507,"storage_full","Could not persist configuration"); return; }
    jsonResponse(web.method()==HTTP_POST ? 201 : 200,existing); return;
  }
  if (web.method()==HTTP_DELETE && !rest.isEmpty()) {
    DynamicJsonDocument previous(MAX_CONFIG_BYTES); previous.set(config);
    for (size_t i=0;i<arr.size();i++) if (arr[i]["id"]==rest) {
      if (key=="devices" && arr[i]["type"]=="relay") { int oldPin=arr[i]["pin"] | -1; commandDevice(rest,"off","configuration"); if (oldPin>=0) pinMode(oldPin,INPUT); }
      arr.remove(i); if (key=="devices") initHardware();
      if (!saveConfig()) { config.set(previous); if (key=="devices") { initHardware(); applyOutputs(); } errorResponse(507,"storage_full","Could not persist configuration"); return; }
      web.send(200,"application/json","{\"ok\":true}"); return;
    }
    errorResponse(404,"not_found","Record not found"); return;
  }
  errorResponse(405,"method_not_allowed","Method not supported");
}
static void staticFile(const String& path, const String& type) {
  File f=LittleFS.open(path,"r"); if (!f) { web.send(404,"text/plain","Web assets missing. Run pio run -t uploadfs"); return; }
  web.sendHeader("Cache-Control","no-cache"); web.streamFile(f,type); f.close();
}
static void handleFallback() {
  String path=web.uri();
  if (path.startsWith("/api/sensors/") && path.endsWith("/history") && web.method()==HTTP_GET) {
    if (!auth()) return;
    String id=path.substring(13,path.length()-8); bool exists=false;
    for (JsonObject d : config["devices"].as<JsonArray>()) if (d["id"]==id && (d["type"]=="analog" || d["type"]=="digital")) exists=true;
    if (!exists) { errorResponse(404,"not_found","Sensor not found"); return; }
    DynamicJsonDocument d(4096); sensorHistory(id,d); jsonResponse(200,d.as<JsonVariantConst>()); return;
  }
  if (path.startsWith("/api/")) {
    if (collectionKey(path).length()) { handleCollection(path); return; }
    errorResponse(404,"not_found","API route not found"); return;
  }
  if (path=="/" || path=="/index.html") { staticFile("/index.html","text/html"); return; }
  if (path=="/app.css") { staticFile("/app.css","text/css"); return; }
  if (path=="/app.js") { staticFile("/app.js","application/javascript"); return; }
  web.send(404,"text/plain","Not found");
}
void initApi() {
  const char* headers[]={"Cookie","Origin","X-CSRF-Token","X-Firmware-SHA256"}; web.collectHeaders(headers,4);
  socket.begin(); socket.onEvent(onSocket);
  web.on("/api/bootstrap",HTTP_GET,[](){ DynamicJsonDocument d(256); d["setup"]=setupMode; d["version"]=VERSION; d["demo"]=false; d["warning"]="LAN HTTP is not encrypted"; jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/setup",HTTP_POST,[](){
    if (!setupMode || !sameOrigin()) { errorResponse(403,"forbidden","Setup unavailable"); return; }
    DynamicJsonDocument b(1024); if (!readJson(b,1024)) return;
    String ssid=b["ssid"] | "", pass=b["wifiPassword"] | "", user=b["username"] | "", password=b["password"] | "";
    if (ssid.length()<1 || ssid.length()>32 || pass.length()>64 || user.length()<3 || user.length()>32 || password.length()<10 || password.length()>128 || String(b["name"] | "My Smart Home").length()>48 || String(b["timezone"] | "UTC0").length()>64) { errorResponse(422,"invalid_setup","Check SSID, username and password lengths"); return; }
    if (!setAdmin(user,password)) { errorResponse(500,"storage_failed","Cannot save administrator"); return; }
    config["name"]=b["name"] | "My Smart Home"; config["timezone"]=b["timezone"] | "UTC0";
    if (!saveConfig()) { errorResponse(507,"storage_full","Could not save configuration"); return; }
    prefs.putString("wifi_pass",pass); prefs.putString("ssid",ssid);
    web.send(200,"application/json","{\"ok\":true,\"restarting\":true}"); restartAt=millis()+2000;
  });
  initSecurity();
  web.on("/api/ws-ticket",HTTP_POST,[](){ if (!auth(true)) return; ticket=randomHex(16); ticketAt=millis(); DynamicJsonDocument d(96); d["ticket"]=ticket; jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/system/status",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(768);
    d["name"]=config["name"]; d["version"]=VERSION; d["uptimeSeconds"]=millis()/1000; d["wifiConnected"]=WiFi.status()==WL_CONNECTED;
    d["ip"]=WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString(); d["rssi"]=WiFi.status()==WL_CONNECTED ? WiFi.RSSI() : 0;
    d["freeHeap"]=ESP.getFreeHeap(); d["freePsram"]=ESP.getFreePsram(); d["rtc"]=rtcAvailable; d["clockValid"]=clockValid; d["sd"]=sdAvailable;
    d["deviceCount"]=config["devices"].as<JsonArray>().size(); d["ruleCount"]=config["automations"].as<JsonArray>().size(); jsonResponse(200,d.as<JsonVariantConst>());
  });
  web.on("/api/system/info",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(256); d["board"]=BOARD; d["firmware"]=VERSION; d["flashBytes"]=ESP.getFlashChipSize(); d["psramBytes"]=ESP.getPsramSize(); jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/system/diagnostics",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(2048); d["board"]=BOARD; d["freeHeap"]=ESP.getFreeHeap(); d["freePsram"]=ESP.getFreePsram(); d["uptimeSeconds"]=millis()/1000; d["events"]=events; jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/system/time",HTTP_POST,[](){ if (!auth(true)) return; DynamicJsonDocument b(256); if (!readJson(b,256)) return; long stamp=b["epoch"] | 0L; if (stamp<1700000000L) { errorResponse(422,"invalid_time","Epoch out of range"); return; } setClock(stamp); web.send(200,"application/json","{\"ok\":true}"); });
  web.on("/api/network/scan",HTTP_GET,[](){
    if (!setupMode && !auth()) return;
    int count=WiFi.scanComplete();
    if (count==WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); count=WIFI_SCAN_RUNNING; }
    DynamicJsonDocument d(2048); d["scanning"]=count==WIFI_SCAN_RUNNING;
    JsonArray list=d.createNestedArray("networks");
    if (count>0) {
      for (int i=0;i<count && i<15;i++) { JsonObject n=list.createNestedObject(); n["ssid"]=WiFi.SSID(i); n["rssi"]=WiFi.RSSI(i); n["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN; }
      WiFi.scanDelete();
    }
    jsonResponse(200,d.as<JsonVariantConst>());
  });
  web.on("/api/network/config",HTTP_POST,[](){
    if (!auth(true)) return;
    DynamicJsonDocument b(256); if (!readJson(b,256)) return;
    String ssid=b["ssid"] | "", password=b["wifiPassword"] | "";
    if (ssid.isEmpty() || ssid.length()>32 || password.length()>64) { errorResponse(422,"invalid_network","SSID or password length invalid"); return; }
    prefs.putString("wifi_pass",password); prefs.putString("ssid",ssid);
    web.send(200,"application/json","{\"ok\":true,\"restarting\":true}"); restartAt=millis()+1500;
  });
  web.on("/api/network/status",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(256); d["connected"]=WiFi.status()==WL_CONNECTED; d["ssid"]=WiFi.SSID(); d["ip"]=WiFi.localIP().toString(); jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/hardware/board",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(2048); d["model"]=BOARD; JsonArray pins=d.createNestedArray("pins"); for (int pin=0;pin<=48;pin++) { JsonObject p=pins.createNestedObject(); p["gpio"]=pin; p["reserved"]=!((pin>=1 && pin<=18 && pin!=8 && pin!=9 && pin!=10 && pin!=11 && pin!=12 && pin!=13) || pin==21 || (pin>=38 && pin<=42) || pin==47); p["adc"]=pin>=1 && pin<=10 && pin!=8 && pin!=9 && pin!=10 && pin!=11 && pin!=12 && pin!=13; } jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/hardware/gpio",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(1024); JsonArray pins=d.to<JsonArray>(); for (JsonObject o : config["devices"].as<JsonArray>()) if (o["type"]!="virtual") { JsonObject p=pins.createNestedObject(); p["gpio"]=o["pin"]; p["deviceId"]=o["id"]; p["name"]=o["name"]; } jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/hardware/test",HTTP_POST,[](){ if (!auth(true)) return; DynamicJsonDocument b(256); if (!readJson(b,256)) return; String id=b["deviceId"] | "", cmd=b["command"] | ""; if (!commandDevice(id,cmd,"hardware test")) { errorResponse(422,"invalid_test","Test a registered controllable device only"); return; } web.send(200,"application/json","{\"ok\":true}"); });
  web.on("/api/dashboard",HTTP_GET,[](){ if (!auth()) return; jsonResponse(200,config["dashboard"].as<JsonVariantConst>()); });
  web.on("/api/dashboard",HTTP_PUT,[](){ if (!auth(true)) return; DynamicJsonDocument b(8192); if (!readJson(b,8192)) return; JsonArrayConst widgets=b["widgets"].as<JsonArrayConst>(); if (widgets.isNull() || widgets.size()>MAX_WIDGETS) { errorResponse(422,"invalid_dashboard","Maximum 48 widgets"); return; }
    for (JsonObjectConst w : widgets) { String type=w["type"] | ""; String id=w["deviceId"] | ""; if ((type!="toggle" && type!="button" && type!="value" && type!="gauge" && type!="chart" && type!="label") || id.length()>24 || (w["x"] | -1)<0 || (w["x"] | 0)>11 || (w["y"] | -1)<0 || (w["w"] | 0)<1 || (w["w"] | 0)>12 || w["x"].as<int>()+w["w"].as<int>()>12 || (w["h"] | 0)<1 || (w["h"] | 0)>6) { errorResponse(422,"invalid_widget","Invalid grid layout or mapping"); return; }
      if (id.length()) { bool compatible=false; for (JsonObject d : config["devices"].as<JsonArray>()) if (d["id"]==id) { bool control=type=="toggle" || type=="button"; compatible=control ? (d["type"]=="relay" || d["type"]=="virtual") : (d["type"]=="analog" || d["type"]=="digital" || d["type"]=="virtual"); } if (!compatible) { errorResponse(422,"incompatible_mapping","Widget and device are incompatible"); return; } }
    }
    DynamicJsonDocument previous(MAX_CONFIG_BYTES); previous.set(config); config["dashboard"].set(b.as<JsonVariantConst>()); if (!saveConfig()) { config.set(previous); errorResponse(507,"storage_full","Could not persist dashboard"); return; } jsonResponse(200,config["dashboard"].as<JsonVariantConst>());
  });
  web.on("/api/events",HTTP_GET,[](){ if (!auth()) return; jsonResponse(200,events.as<JsonVariantConst>()); });
  web.on("/api/sensors",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(4096); JsonArray a=d.to<JsonArray>(); for (JsonObject o : config["devices"].as<JsonArray>()) if (o["type"]=="analog" || o["type"]=="digital") a.add(o); jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/scenes/activate",HTTP_POST,[](){ if (!auth(true)) return; DynamicJsonDocument b(256); if (!readJson(b,256)) return; if (!activateScene(b["id"] | "")) { errorResponse(404,"not_found","Scene not found"); return; } web.send(200,"application/json","{\"ok\":true}"); });
  web.on("/api/storage/status",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(256); d["littlefsTotal"]=LittleFS.totalBytes(); d["littlefsUsed"]=LittleFS.usedBytes(); d["sdAvailable"]=sdAvailable; d["historyPersistent"]=sdAvailable; jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/storage/backup",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(MAX_CONFIG_BYTES); d.set(config); d["backupVersion"]=1; d["exportedAt"]=clockValid ? (long)time(nullptr) : 0; web.sendHeader("Content-Disposition","attachment; filename=shap-backup.json"); jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/storage/restore",HTTP_POST,[](){ if (!auth(true)) return; DynamicJsonDocument b(MAX_CONFIG_BYTES); if (!readJson(b,MAX_CONFIG_BYTES-512)) return;
    if (b["schema"]!=1 || b["board"]!=BOARD || b["backupVersion"]!=1 || !b["devices"].is<JsonArray>() || !b["dashboard"]["widgets"].is<JsonArray>()) { errorResponse(422,"incompatible_backup","Invalid version or board"); return; }
    if (!b["automations"].is<JsonArray>() || !b["scenes"].is<JsonArray>() || !b["schedules"].is<JsonArray>() || b["devices"].as<JsonArray>().size()>MAX_DEVICES || b["automations"].as<JsonArray>().size()>MAX_RULES || b["scenes"].as<JsonArray>().size()>MAX_SCENES || b["schedules"].as<JsonArray>().size()>MAX_SCHEDULES || b["dashboard"]["widgets"].as<JsonArray>().size()>MAX_WIDGETS) { errorResponse(422,"limit_reached","Backup exceeds limits"); return; }
    DynamicJsonDocument old(MAX_CONFIG_BYTES); old.set(config); config.set(b); config.remove("backupVersion"); config.remove("exportedAt");
    String issue;
    for (JsonObject o : config["devices"].as<JsonArray>()) if (!validIdentifier(o["id"] | "") || !validateDevice(o,o["id"] | "",issue)) { config.set(old); errorResponse(422,"invalid_device",issue.length()?issue:"Invalid device identifier"); return; }
    for (const char* key : {"automations","scenes","schedules"}) for (JsonObject o : config[key].as<JsonArray>()) if (!validIdentifier(o["id"] | "") || !validateRecord(key,o,o["id"] | "",issue)) { config.set(old); errorResponse(422,"invalid_record",issue.length()?issue:"Invalid identifier"); return; }
    for (JsonObject w : config["dashboard"]["widgets"].as<JsonArray>()) {
      String type=w["type"] | "", id=w["deviceId"] | "";
      if (!validIdentifier(w["id"] | "") || (type!="toggle" && type!="button" && type!="value" && type!="gauge" && type!="chart" && type!="label") || (w["x"] | -1)<0 || (w["x"] | 0)>11 || (w["w"] | 0)<1 || (w["w"] | 0)>12 || (w["x"].as<int>()+w["w"].as<int>())>12 || (w["y"] | -1)<0 || (w["h"] | 0)<1 || (w["h"] | 0)>6) { config.set(old); errorResponse(422,"invalid_widget","Invalid dashboard layout"); return; }
      String target=deviceType(id);
      if (id.length() && ((type=="toggle" || type=="button") ? (target!="relay" && target!="virtual") : (target!="analog" && target!="digital" && target!="virtual"))) { config.set(old); errorResponse(422,"incompatible_mapping","Widget mapping incompatible"); return; }
    }
    // Validate conflicts in the imported collection, not against only the existing registry.
    JsonArray devices=config["devices"].as<JsonArray>(); for (size_t i=0;i<devices.size();i++) for (size_t j=i+1;j<devices.size();j++) if (devices[i]["id"]==devices[j]["id"] || (devices[i]["type"]!="virtual" && devices[j]["type"]!="virtual" && devices[i]["pin"]==devices[j]["pin"])) { config.set(old); errorResponse(422,"gpio_conflict","Duplicate GPIO in backup"); return; }
    if (!saveConfig()) { config.set(old); errorResponse(507,"storage_full","Restore failed"); return; }
    web.send(200,"application/json","{\"ok\":true,\"restarting\":true}"); restartAt=millis()+1500;
  });
  web.on("/api/firmware/version",HTTP_GET,[](){ if (!auth()) return; DynamicJsonDocument d(128); d["version"]=VERSION; d["signedFirmware"]=false; jsonResponse(200,d.as<JsonVariantConst>()); });
  web.on("/api/firmware/update",HTTP_POST,[](){
    if (!auth(true)) { if (updateOk) Update.abort(); updateOk=false; return; }
    if (!updateOk || !hashStarted) { if (hashStarted) { mbedtls_sha256_free(&updateHash); hashStarted=false; } Update.abort(); errorResponse(422,"update_failed","Upload incomplete or invalid"); return; }
    uint8_t hash[32]; mbedtls_sha256_finish_ret(&updateHash,hash); mbedtls_sha256_free(&updateHash); hashStarted=false;
    String hex; const char* digits="0123456789abcdef"; for (uint8_t v : hash) { hex+=digits[v>>4]; hex+=digits[v&15]; }
    if (hex!=updateExpected) { Update.abort(); updateOk=false; errorResponse(422,"hash_mismatch","Firmware SHA-256 mismatch; update aborted"); return; }
    if (!Update.end(true)) { updateOk=false; errorResponse(422,"update_failed",Update.errorString()); return; }
    updateOk=false; web.send(200,"application/json","{\"ok\":true,\"restarting\":true}"); restartAt=millis()+2000;
  },[](){
    if (!authorized(true)) return;
    HTTPUpload& u=web.upload();
    if (u.status==UPLOAD_FILE_START) {
      updateOk=false; if (hashStarted) { mbedtls_sha256_free(&updateHash); hashStarted=false; }
      updateExpected=web.header("X-Firmware-SHA256");
      if (updateExpected.length()==64 && u.filename.endsWith(".bin")) {
        mbedtls_sha256_init(&updateHash); hashStarted=true; mbedtls_sha256_starts_ret(&updateHash,0);
        updateOk=Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);
      }
    } else if (u.status==UPLOAD_FILE_WRITE && updateOk) {
      mbedtls_sha256_update_ret(&updateHash,u.buf,u.currentSize);
      updateOk=Update.write(u.buf,u.currentSize)==u.currentSize;
      if (!updateOk) Update.abort();
    } else if (u.status==UPLOAD_FILE_ABORTED) {
      Update.abort(); updateOk=false;
      if (hashStarted) { mbedtls_sha256_free(&updateHash); hashStarted=false; }
    }
  });
  web.onNotFound(handleFallback); web.begin();
}
void tickApi() { if (ticket.length() && millis()-ticketAt>30000) ticket=""; }
}
