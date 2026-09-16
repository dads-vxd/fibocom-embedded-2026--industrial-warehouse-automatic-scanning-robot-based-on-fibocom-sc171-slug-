#include "qr_scanner.h"

#include <opencv2/wechat_qrcode.hpp>

struct QRScanner::Impl {
  cv::Ptr<cv::wechat_qrcode::WeChatQRCode> detector;
};

QRScanner::~QRScanner() = default;

QRScanner::QRScanner(const std::string &detectProto,
                     const std::string &detectModel,
                     const std::string &srProto,
                     const std::string &srModel) {
  impl_ = std::make_unique<Impl>();
  impl_->detector = cv::makePtr<cv::wechat_qrcode::WeChatQRCode>(
      detectProto, detectModel, srProto, srModel);
}

std::vector<QRResult> QRScanner::scan(const cv::Mat &image) {
  std::vector<QRResult> results;

  if (image.empty())
    return results;

  std::vector<cv::Mat> points;
  std::vector<std::string> decoded =
      impl_->detector->detectAndDecode(image, points);

  for (const auto &s : decoded) {
    if (!s.empty()) {
      QRResult r;
      r.data = s;
      results.push_back(std::move(r));
    }
  }
  return results;
}
