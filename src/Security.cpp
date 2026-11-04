#include "Platform.h"
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
namespace shap {
struct Session { String token, csrf; unsigned long expires; };
static Session sessions[3];
static unsigned long failedAt = 0;
static unsigned int failedCount = 0;
String randomHex(size_t bytes) {
  static const char digits[] = "0123456789abcdef";
  String out; out.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) { uint8_t b = (uint8_t)esp_random(); out += digits[b >> 4]; out += digits[b & 15]; }
  return out;
}
static bool digest(const String& password, const String& salt, String& result) {
  uint8_t saltBytes[16], hash[32];
  if (salt.length() != 32) return false;
  for (int i = 0; i < 16; i++) saltBytes[i] = (uint8_t)strtoul(salt.substring(i*2, i*2+2).c_str(), nullptr, 16);
  mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) != 0) { mbedtls_md_free(&ctx); return false; }
  int rc = mbedtls_pkcs5_pbkdf2_hmac(&ctx, (const unsigned char*)password.c_str(), password.length(), saltBytes, 16, 100000, 32, hash);
  mbedtls_md_free(&ctx); if (rc != 0) return false;
  static const char hex[] = "0123456789abcdef"; result = ""; result.reserve(64);
  for (uint8_t b : hash) { result += hex[b >> 4]; result += hex[b & 15]; }
  return true;
}
static bool constantEqual(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0; for (size_t i=0; i<a.length(); i++) diff |= a[i] ^ b[i]; return diff == 0;
}
void invalidateSessions() { for (auto& s : sessions) s = {}; }
static String cookieToken() {
  String cookie = web.header("Cookie"); int p = cookie.indexOf("shap_session=");
  if (p < 0) return ""; p += 13; int end = cookie.indexOf(';', p);
  return cookie.substring(p, end < 0 ? cookie.length() : end);
}
bool authorized(bool mutation) {
  String token = cookieToken();
  for (auto& s : sessions) {
    if (s.token.length() && constantEqual(token, s.token) && (long)(s.expires - millis()) > 0) {
      if (mutation) {
        if (!constantEqual(web.header("X-CSRF-Token"), s.csrf)) return false;
        String origin = web.header("Origin");
        if (origin.length() && origin != "http://" + web.hostHeader()) return false;
      }
      s.expires = millis() + 28800000UL; return true;
    }
  }
  return false;
}
void initSecurity() {
  web.on("/api/auth/session", HTTP_GET, []() {
    DynamicJsonDocument d(256); bool ok = authorized(); d["authenticated"] = ok;
    if (ok) for (auto& s : sessions) if (s.token == cookieToken()) d["csrf"] = s.csrf;
    jsonResponse(200, d.as<JsonVariantConst>());
  });
  web.on("/api/auth/login", HTTP_POST, []() {
    if (setupMode) { errorResponse(403,"setup_required","Finish setup first"); return; }
    if (failedCount >= 5 && millis() - failedAt < 60000) { errorResponse(429,"rate_limited","Try again in one minute"); return; }
    DynamicJsonDocument body(512); if (!readJson(body, 512)) return;
    String hash; bool ok = body["username"].as<String>() == prefs.getString("admin", "") && digest(body["password"].as<String>(), prefs.getString("salt", ""), hash) && constantEqual(hash, prefs.getString("hash", ""));
    if (!ok) { failedCount = millis() - failedAt > 60000 ? 1 : failedCount + 1; failedAt = millis(); errorResponse(401,"invalid_credentials","Invalid credentials"); return; }
    failedCount = 0; Session& s = sessions[0]; s.token = randomHex(32); s.csrf = randomHex(16); s.expires = millis() + 28800000UL;
    web.sendHeader("Set-Cookie", "shap_session=" + s.token + "; HttpOnly; SameSite=Strict; Path=/; Max-Age=28800");
    DynamicJsonDocument d(128); d["csrf"] = s.csrf; d["authenticated"] = true; jsonResponse(200, d.as<JsonVariantConst>());
  });
  web.on("/api/auth/logout", HTTP_POST, []() {
    if (!authorized(true)) { errorResponse(403,"forbidden","Invalid session or CSRF token"); return; }
    invalidateSessions(); web.sendHeader("Set-Cookie", "shap_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
    web.send(200,"application/json","{\"ok\":true}");
  });
  web.on("/api/auth/password", HTTP_POST, []() {
    if (!authorized(true)) { errorResponse(403,"forbidden","Invalid session or CSRF token"); return; }
    DynamicJsonDocument body(512); if (!readJson(body, 512)) return;
    String oldHash; if (!digest(body["current"].as<String>(), prefs.getString("salt", ""), oldHash) || !constantEqual(oldHash, prefs.getString("hash", ""))) { errorResponse(403,"forbidden","Incorrect current password"); return; }
    String pw = body["password"].as<String>(); if (pw.length()<10 || pw.length()>128) { errorResponse(400,"invalid_password","Password must be 10–128 characters"); return; }
    String salt=randomHex(16), hash; if (!digest(pw,salt,hash)) { errorResponse(500,"hash_failed","Password hashing failed"); return; }
    prefs.putString("salt",salt); prefs.putString("hash",hash); invalidateSessions(); web.send(200,"application/json","{\"ok\":true}");
  });
}
bool setAdmin(const String& name, const String& password) {
  if (name.length()<3 || name.length()>32 || password.length()<10 || password.length()>128) return false;
  String salt=randomHex(16), hash; if (!digest(password,salt,hash)) return false;
  return prefs.putString("admin",name) && prefs.putString("salt",salt) && prefs.putString("hash",hash);
}
}
