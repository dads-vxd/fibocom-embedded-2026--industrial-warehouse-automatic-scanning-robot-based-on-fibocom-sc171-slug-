#include "drawing.h"

#include <string>
#include <unordered_map>

cv::Scalar trackIdToColor(int id) {
  static cv::RNG rng(42);
  static std::unordered_map<int, cv::Scalar> cache;
  auto it = cache.find(id);
  if (it != cache.end())
    return it->second;
  cv::Scalar c(rng.uniform(50, 256), rng.uniform(50, 256),
               rng.uniform(50, 256));
  cache[id] = c;
  return c;
}

bool isBlurry(const cv::Mat &grayImg, double threshold) {
  cv::Mat laplacian;
  cv::Laplacian(grayImg, laplacian, CV_64F);
  cv::Scalar mean, stddev;
  cv::meanStdDev(laplacian, mean, stddev);
  return stddev.val[0] * stddev.val[0] < threshold;
}

void drawTrackInfo(cv::Mat &frame, cv::Point2f bottomVertex,
                   cv::Point2f center, int trackId,
                   const cv::Scalar &color,
                   const TrackDisplayInfo &info) {
  int baseline = 0;

  cv::Size bs =
      cv::getTextSize(info.barcodeText, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1,
                      &baseline);
  int bx = static_cast<int>(bottomVertex.x);
  int by = static_cast<int>(bottomVertex.y) + bs.height + 10;
  cv::rectangle(frame, cv::Point(bx - 1, by - bs.height - 1),
                cv::Point(bx + bs.width + 1, by + 2), color, -1);
  cv::putText(frame, info.barcodeText, cv::Point(bx, by),
              cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);

  if (info.isUploaded) {
    std::string upLabel = "UPLOADED";
    cv::Size us = cv::getTextSize(upLabel, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1,
                                  &baseline);
    int ux = bx;
    int uy = by + us.height + 6;
    cv::rectangle(frame, cv::Point(ux - 1, uy - us.height - 1),
                  cv::Point(ux + us.width + 1, uy + 2),
                  cv::Scalar(0, 200, 0), -1);
    cv::putText(frame, upLabel, cv::Point(ux, uy),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
  } else if (!info.matchStatus.empty()) {
    cv::Size ms =
        cv::getTextSize(info.matchStatus, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1,
                        &baseline);
    int mx = bx;
    int my = by + ms.height + 6;
    cv::rectangle(frame, cv::Point(mx - 1, my - ms.height - 1),
                  cv::Point(mx + ms.width + 1, my + 2),
                  cv::Scalar(0, 255, 255), -1);
    cv::putText(frame, info.matchStatus, cv::Point(mx, my),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
  }

  cv::Point cp(static_cast<int>(center.x), static_cast<int>(center.y));
  std::string idStr = std::to_string(trackId);
  cv::Size ts =
      cv::getTextSize(idStr, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &baseline);
  int r = std::max(ts.width, ts.height) / 2 + 6;
  cv::circle(frame, cp, r, color, -1, cv::LINE_AA);
  cv::circle(frame, cp, r, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
  cv::putText(frame, idStr,
              cv::Point(cp.x - ts.width / 2, cp.y + ts.height / 2),
              cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2,
              cv::LINE_AA);
}
