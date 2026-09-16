#include "iou_utils.h"

#include <algorithm>

float computeIoU(const cv::Rect_<float> &a, const cv::Rect_<float> &b) {
  float x1 = std::max(a.x, b.x);
  float y1 = std::max(a.y, b.y);
  float x2 = std::min(a.x + a.width, b.x + b.width);
  float y2 = std::min(a.y + a.height, b.y + b.height);

  float interW = std::max(0.0f, x2 - x1);
  float interH = std::max(0.0f, y2 - y1);
  float inter = interW * interH;

  float areaA = a.width * a.height;
  float areaB = b.width * b.height;
  float unionArea = areaA + areaB - inter;
  return unionArea > 0.0f ? inter / unionArea : 0.0f;
}

float computeRotatedIoU(const cv::RotatedRect &a, const cv::RotatedRect &b) {
  std::vector<cv::Point2f> inter;
  int result = cv::rotatedRectangleIntersection(a, b, inter);
  if (result == cv::INTERSECT_NONE || inter.empty())
    return 0.f;

  float interArea = static_cast<float>(cv::contourArea(inter));
  float areaA = a.size.width * a.size.height;
  float areaB = b.size.width * b.size.height;
  float unionArea = areaA + areaB - interArea;
  return unionArea > 0.f ? interArea / unionArea : 0.f;
}
