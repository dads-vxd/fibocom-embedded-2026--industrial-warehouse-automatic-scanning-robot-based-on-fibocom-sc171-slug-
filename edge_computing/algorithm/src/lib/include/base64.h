#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::string base64Encode(const std::vector<uint8_t> &data);
std::string base64Encode(const uint8_t *data, size_t len);
