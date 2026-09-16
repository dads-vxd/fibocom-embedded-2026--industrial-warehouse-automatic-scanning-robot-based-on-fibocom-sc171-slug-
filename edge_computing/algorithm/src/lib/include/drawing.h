#pragma once

#include <opencv2/opencv.hpp>
#include <string>

cv::Scalar trackIdToColor(int id);

bool isBlurry(const cv::Mat &grayImg, double threshold = 100.0);

struct TrackDisplayInfo {
  std::string barcodeText;
  std::string matchStatus;
  bool isUploaded = false;
};

void drawTrackInfo(cv::Mat &frame, cv::Point2f bottomVertex,
                   cv::Point2f center, int trackId,
                   const cv::Scalar &color,
                   const TrackDisplayInfo &info);
