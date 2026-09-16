#include "qr_scanner.h"

#include <iostream>
#include <opencv2/opencv.hpp>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <image_path>" << std::endl;
    return 1;
  }

  std::string imagePath = argv[1];

  cv::Mat image = cv::imread(imagePath);
  if (image.empty()) {
    std::cerr << "Cannot read image: " << imagePath << std::endl;
    return 1;
  }

  std::cout << "Image: " << imagePath << " (" << image.cols << "x"
            << image.rows << ")" << std::endl;

  QRScanner scanner("./models/detect.prototxt", "./models/detect.caffemodel",
                    "./models/sr.prototxt", "./models/sr.caffemodel");

  auto results = scanner.scan(image);

  if (results.empty()) {
    std::cout << "No QR code detected" << std::endl;
    return 0;
  }

  std::cout << "Found " << results.size() << " QR code(s):" << std::endl;
  for (size_t i = 0; i < results.size(); ++i) {
    std::cout << "  [" << i << "] " << results[i].data << std::endl;
  }

  return 0;
}
