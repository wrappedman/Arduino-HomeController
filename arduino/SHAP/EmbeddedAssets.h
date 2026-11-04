#pragma once
#include <Arduino.h>
namespace shap {
struct EmbeddedAsset {
  const char* path;
  const char* mime;
  const uint8_t* data;
  size_t length;
  bool gzipped;
};
const EmbeddedAsset* findEmbeddedAsset(const String& path);
}
