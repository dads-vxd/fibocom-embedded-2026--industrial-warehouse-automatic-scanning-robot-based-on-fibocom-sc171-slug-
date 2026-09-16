#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

struct BarcodeEntry {
  int64_t categoryId;
  nlohmann::json fields;
};

using BarcodeDataMap = std::unordered_map<std::string, BarcodeEntry>;

BarcodeDataMap loadBarcodeData(const std::string &path);

std::string extractBarcodeData(const std::string &label);
