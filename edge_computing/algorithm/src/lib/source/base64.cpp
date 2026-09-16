#include "base64.h"

static const char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t> &data) {
  return base64Encode(data.data(), data.size());
}

std::string base64Encode(const uint8_t *data, size_t len) {
  std::string result;
  result.reserve(((len + 2) / 3) * 4);

  size_t i = 0;
  for (; i + 2 < len; i += 3) {
    uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
    result += kBase64Table[(n >> 18) & 0x3F];
    result += kBase64Table[(n >> 12) & 0x3F];
    result += kBase64Table[(n >> 6) & 0x3F];
    result += kBase64Table[n & 0x3F];
  }

  if (i < len) {
    uint32_t n = data[i] << 16;
    if (i + 1 < len)
      n |= data[i + 1] << 8;
    result += kBase64Table[(n >> 18) & 0x3F];
    result += kBase64Table[(n >> 12) & 0x3F];
    if (i + 1 < len)
      result += kBase64Table[(n >> 6) & 0x3F];
    else
      result += '=';
    result += '=';
  }

  return result;
}
