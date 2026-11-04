#include "Platform.h"
namespace shap {
bool setupMode=true, rtcAvailable=false, clockValid=false, sdAvailable=false;
unsigned long bootMs=0, restartAt=0;
static unsigned long reconnectAt=0;
static bool apRunning=false;
static bool reportedIp=false;
static void startAP() {
  String suffix=String((uint32_t)(ESP.getEfuseMac() & 0xffff),HEX); suffix.toUpperCase();
  String password=prefs.getString("ap_pass","");
  if (password.length()<12) { password=randomHex(8); prefs.putString("ap_pass",password); }
  WiFi.mode(WIFI_AP_STA); WiFi.softAP(("SmartHome-Setup-" + suffix).c_str(),password.c_str()); apRunning=true;
  Serial.println("SETUP AP: SmartHome-Setup-" + suffix + " PASSWORD: " + password + " ADDRESS: http://192.168.4.1");
}
void initNetwork() {
  WiFi.setSleep(false);
  setupMode=prefs.getString("ssid","").isEmpty() || prefs.getString("admin","").isEmpty();
  if (setupMode) { startAP(); return; }
  WiFi.mode(WIFI_STA); WiFi.setHostname("shap-controller");
  WiFi.begin(prefs.getString("ssid","").c_str(),prefs.getString("wifi_pass","").c_str()); reconnectAt=millis();
}
void tickNetwork() {
  if (setupMode) return;
  if (WiFi.status()==WL_CONNECTED) {
    if (!reportedIp) { Serial.println("SHAP LAN IP: " + WiFi.localIP().toString()); reportedIp=true; }
    if (apRunning) { WiFi.softAPdisconnect(true); apRunning=false; }
    return;
  }
  reportedIp=false;
  if (millis()-reconnectAt>15000) {
    reconnectAt=millis(); WiFi.reconnect();
    if (!apRunning && millis()-bootMs>45000) startAP(); // recovery AP; main API remains authenticated
  }
}
// Press BOOT for five continuous seconds *after* firmware has started.
// No busy wait: control and automation continue during the hold.
void tickRecoveryButton() {
  static unsigned long pressedAt=0;
  static bool fired=false;
  if (millis()-bootMs<2000 || setupMode) return;
  if (digitalRead(0)==HIGH) { pressedAt=0; fired=false; return; }
  if (!pressedAt) pressedAt=millis();
  if (!fired && millis()-pressedAt>=5000) {
    fired=true;
    prefs.remove("ssid"); prefs.remove("wifi_pass");
    Serial.println("BOOT held after startup: network credentials cleared; restarting into setup AP");
    recordEvent("Physical network recovery requested");
    restartAt=millis()+500;
  }
}

}
