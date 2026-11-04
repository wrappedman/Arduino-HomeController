#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Update.h>
#include <Wire.h>
#include <time.h>

namespace shap {
constexpr const char* VERSION = "0.1.0-preview";
constexpr const char* BOARD = "ESP32-S3-DevKitC-1-N16R8";
constexpr size_t MAX_CONFIG_BYTES = 32768;
constexpr int MAX_DEVICES = 24, MAX_RULES = 24, MAX_SCENES = 12, MAX_WIDGETS = 48, MAX_SCHEDULES = 24;
extern DynamicJsonDocument config;
extern WebServer web;
extern WebSocketsServer socket;
extern Preferences prefs;
extern bool setupMode;
extern bool rtcAvailable;
extern bool clockValid;
extern bool sdAvailable;
extern unsigned long bootMs;
extern unsigned long restartAt;
String randomHex(size_t bytes);
void jsonResponse(int code, JsonVariantConst value);
void errorResponse(int code, const String& codeName, const String& message);
bool readJson(DynamicJsonDocument& out, size_t limit = 8192);
bool loadConfig();
bool saveConfig();
void ensureConfig();
void initHardware();
void tickHardware();
void applyOutputs();
bool validateDevice(JsonVariantConst device, const String& ignoreId, String& error);
bool commandDevice(const String& id, const String& command, const String& source);
void recordEvent(const String& message, const String& category = "system");
void tickAutomation();
bool activateScene(const String& id);
void deviceChanged(const String& id, float value);
void initNetwork();
void setClock(time_t stamp);
void tickNetwork();
void tickRecoveryButton();
void initApi();
void tickApi();
void initSecurity();
bool setAdmin(const String& name, const String& password);
bool authorized(bool mutation = false);
void invalidateSessions();
}
namespace shap {
extern DynamicJsonDocument events;
void broadcastState(const String& id, float value);
void broadcastMessage(const String& payload);
}
namespace shap {
void initHistory();
void tickHistory();
void sampleHistory(const String& id, float value);
void sensorHistory(const String& id, DynamicJsonDocument& result);
}
