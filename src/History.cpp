#include "Platform.h"
#include <SD.h>
#include <SPI.h>
namespace shap {
// Fixed SPI wiring for the optional 3.3 V microSD module: CS10, MOSI11, SCK12, MISO13.
// Files rotate at 256 KiB per sensor. RAM ring is bounded and remains available without SD.
struct Sample { char id[17]; float value; uint32_t epoch, uptime; };
static Sample ring[128], pending[32];
static uint8_t ringHead=0, ringCount=0, qHead=0, qTail=0;
static unsigned long retryAt=0;
static String pathFor(const String& id) {
  if (id.isEmpty() || id.length()>16) return "";
  for (size_t i=0;i<id.length();i++) if (!isAlphaNumeric(id[i]) && id[i]!='_') return "";
  return "/h_" + id + ".csv";
}
void initHistory() {
  SPI.begin(12,13,11,10);
  sdAvailable=SD.begin(10,SPI,10000000); retryAt=millis();
  recordEvent(sdAvailable ? "microSD history available" : "microSD absent; using bounded RAM history");
}
void sampleHistory(const String& id, float value) {
  if (pathFor(id).isEmpty()) return;
  Sample s={}; id.toCharArray(s.id,sizeof(s.id)); s.value=value;
  s.epoch=clockValid ? (uint32_t)time(nullptr) : 0; s.uptime=millis()/1000;
  ring[ringHead]=s; ringHead=(ringHead+1)%128; if (ringCount<128) ringCount++;
  uint8_t next=(qTail+1)%32;
  if (next==qHead) qHead=(qHead+1)%32; // bounded; old measurements are dropped first
  pending[qTail]=s; qTail=next;
}
void tickHistory() {
  if (!sdAvailable && millis()-retryAt>60000) {
    retryAt=millis(); sdAvailable=SD.begin(10,SPI,10000000);
    if (sdAvailable) recordEvent("microSD reconnected");
  }
  if (!sdAvailable || qHead==qTail) return;
  Sample s=pending[qHead]; qHead=(qHead+1)%32;
  String path=pathFor(s.id); File existing=SD.open(path,FILE_READ);
  size_t size=existing ? existing.size() : 0; if (existing) existing.close();
  if (size>262144) { SD.remove(path); recordEvent("Sensor history rotated: " + String(s.id)); }
  File f=SD.open(path,FILE_APPEND);
  if (!f) { sdAvailable=false; retryAt=millis(); recordEvent("microSD write failed; using RAM ring","error"); return; }
  f.printf("%lu,%lu,%.3f\n",(unsigned long)s.epoch,(unsigned long)s.uptime,s.value);
  f.close();
}
void sensorHistory(const String& id, DynamicJsonDocument& result) {
  JsonArray arr=result.to<JsonArray>();
  String path=pathFor(id); if (path.isEmpty()) return;
  if (sdAvailable) {
    File f=SD.open(path,FILE_READ);
    if (f) {
      // Bound the response to the last 48 rows. The file is bounded to 256 KiB.
      String lines[48]; size_t count=0;
      while (f.available()) { String line=f.readStringUntil('\n'); if (line.length()<3 || line.length()>64) continue; lines[count++%48]=line; }
      f.close();
      size_t total=min(count,(size_t)48);
      for (size_t i=0;i<total;i++) {
        String line=lines[(count-total+i)%48]; unsigned long epoch=0,uptime=0; float value=0;
        if (sscanf(line.c_str(),"%lu,%lu,%f",&epoch,&uptime,&value)!=3) continue;
        JsonObject obj=arr.createNestedObject(); obj["at"]=epoch; obj["uptime"]=uptime; obj["value"]=value;
      }
      return;
    }
  }
  uint8_t selected[48]; size_t matches=0;
  for (size_t i=0;i<ringCount;i++) {
    uint8_t index=(ringHead+128-ringCount+i)%128;
    if (id==ring[index].id) selected[matches++%48]=index;
  }
  size_t n=min(matches,(size_t)48);
  for (size_t i=0;i<n;i++) {
    Sample& s=ring[selected[(matches-n+i)%48]];
    JsonObject obj=arr.createNestedObject(); obj["at"]=s.epoch; obj["uptime"]=s.uptime; obj["value"]=s.value;
  }
}
}
