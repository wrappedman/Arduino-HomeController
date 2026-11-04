#include "Platform.h"
#include <sys/time.h>
#include <cstdlib>
namespace shap {
// DS3231 on GPIO8 SDA / GPIO9 SCL. Optional, detected by ACK at 0x68.
static uint8_t bcd(uint8_t v) { return (v >> 4)*10 + (v & 15); }
static uint8_t toBcd(uint8_t v) { return (v/10)*16+(v%10); }
static void setZone() { setenv("TZ", config["timezone"] | "UTC0",1); tzset(); }
void initRtc() {
  setZone(); Wire.begin(8,9);
  Wire.beginTransmission(0x68); rtcAvailable=Wire.endTransmission()==0;
  if (!rtcAvailable) return;
  Wire.beginTransmission(0x68); Wire.write((uint8_t)0); if (Wire.endTransmission()!=0) return;
  if (Wire.requestFrom(0x68,7)!=7) return;
  int sec=bcd(Wire.read() & 0x7f), min=bcd(Wire.read()), hour=bcd(Wire.read() & 0x3f);
  Wire.read(); int day=bcd(Wire.read()), month=bcd(Wire.read() & 0x1f), year=2000+bcd(Wire.read());
  if (year<2024 || month<1 || month>12 || day<1 || day>31) return;
  tm t={}; t.tm_sec=sec; t.tm_min=min; t.tm_hour=hour; t.tm_mday=day; t.tm_mon=month-1; t.tm_year=year-1900;
  timeval tv={mktime(&t),0}; settimeofday(&tv,nullptr); clockValid=true;
}
void setClock(time_t stamp) {
  timeval tv={stamp,0}; settimeofday(&tv,nullptr); clockValid=true; setZone();
  if (!rtcAvailable) return;
  tm t; localtime_r(&stamp,&t);
  Wire.beginTransmission(0x68); Wire.write((uint8_t)0);
  Wire.write(toBcd(t.tm_sec)); Wire.write(toBcd(t.tm_min)); Wire.write(toBcd(t.tm_hour));
  Wire.write(toBcd(t.tm_wday+1)); Wire.write(toBcd(t.tm_mday)); Wire.write(toBcd(t.tm_mon+1)); Wire.write(toBcd((t.tm_year+1900)%100)); Wire.endTransmission();
}
}
