#include "Platform.h"
namespace shap { extern void initRtc(); }
void setup() {
  Serial.begin(115200); delay(200);
  shap::bootMs=millis(); shap::prefs.begin("shap",false);
  shap::loadConfig();
  // Hold BOOT / GPIO0 at power-on to clear only network credentials (not administrator/config).
  pinMode(0,INPUT_PULLUP);
  if (digitalRead(0)==LOW) {
    unsigned long start=millis(); while (digitalRead(0)==LOW && millis()-start<5000) delay(10);
    if (millis()-start>=5000) { shap::prefs.remove("ssid"); shap::prefs.remove("wifi_pass"); Serial.println("Network credentials cleared"); }
  }
  shap::initHardware(); shap::initHistory(); shap::initNetwork(); shap::initRtc(); shap::initApi();
  Serial.println("SHAP " + String(shap::VERSION) + " ready");
}
void loop() {
  shap::web.handleClient(); shap::socket.loop(); shap::tickHardware(); shap::tickHistory(); shap::tickAutomation(); shap::tickNetwork(); shap::tickApi();
  if (shap::restartAt && (long)(millis()-shap::restartAt)>=0) ESP.restart();
  delay(1);
}
