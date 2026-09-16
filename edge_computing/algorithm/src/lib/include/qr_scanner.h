#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

struct QRResult {
  std::string data;
};

class QRScanner {
public:
  QRScanner(const std::string &detectProto, const std::string &detectModel,
            const std::string &srProto, const std::string &srModel);
  ~QRScanner();

  std::vector<QRResult> scan(const cv::Mat &image);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
