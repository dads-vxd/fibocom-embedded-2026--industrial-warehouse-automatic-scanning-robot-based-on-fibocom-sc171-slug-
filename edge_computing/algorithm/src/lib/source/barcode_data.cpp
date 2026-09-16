#include "barcode_data.h"

#include <fstream>
#include <iostream>

static std::string stripJsonComments(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      while (i < s.size() && s[i] != '\n')
        ++i;
      if (i < s.size())
        out += '\n';
    } else {
      out += s[i];
    }
  }
  return out;
}

BarcodeDataMap loadBarcodeData(const std::string &path) {
  BarcodeDataMap map;
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    std::cerr << "[Data] Cannot open " << path << std::endl;
    return map;
  }
  std::string raw((std::istreambuf_iterator<char>(ifs)),
                  std::istreambuf_iterator<char>());
  try {
    auto j = nlohmann::json::parse(stripJsonComments(raw));
    for (const auto &entry : j) {
      BarcodeEntry be;
      be.categoryId = entry["category_id"].get<int64_t>();
      be.fields = entry["fields"];
      map[entry["data"].get<std::string>()] = std::move(be);
    }
    std::cout << "[Data] Loaded " << map.size() << " barcode entries from "
              << path << std::endl;
    for (const auto &kv : map)
      std::cout << "  key: [" << kv.first << "]" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[Data] Parse error: " << e.what() << std::endl;
  }
  return map;
}

std::string extractBarcodeData(const std::string &label) {
  size_t colon = label.find(':');
  std::string raw = (colon == std::string::npos) ? label
                                                  : label.substr(colon + 1);
  // strip GS1 separator characters (GS=0x1D, RS=0x1E, EOT=0x04)
  std::string out;
  out.reserve(raw.size());
  for (char c : raw) {
    if (c != '\x1D' && c != '\x1E' && c != '\x04')
      out += c;
  }
  return out;
}
