#include "Platform.h"
namespace shap { extern void initRtc(); }
void setup() {
  Serial.begin(115200); delay(200);
  shap::bootMs=millis(); shap::prefs.begin("shap",false);
  shap::loadConfig();
  // GPIO0 is a boot strap pin: holding it during reset enters ROM download mode.
  // Network recovery is sampled non-blockingly AFTER boot in tickRecoveryButton().
  pinMode(0,INPUT_PULLUP);
  shap::initHardware(); shap::initHistory(); shap::initNetwork(); shap::initRtc(); shap::initApi();
  Serial.println("SHAP " + String(shap::VERSION) + " ready");
}
void loop() {
  shap::web.handleClient(); shap::socket.loop(); shap::tickHardware(); shap::tickHistory(); shap::tickAutomation(); shap::tickRecoveryButton(); shap::tickNetwork(); shap::tickApi();
  if (shap::restartAt && (long)(millis()-shap::restartAt)>=0) ESP.restart();
  delay(1);
}
