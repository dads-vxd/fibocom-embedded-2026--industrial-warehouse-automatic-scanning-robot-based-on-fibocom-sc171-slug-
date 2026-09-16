#pragma once

#include <memory>
#include <opencv2/opencv.hpp>
#include <utility>
#include <vector>
#include "yolov11.h"

struct TrackObject {
  cv::RotatedRect rbox;
  float confidence;
  int classId;
  int trackId;
};

class BYTETracker {
public:
  BYTETracker(int frameRate = 30, int trackBuffer = 30,
              float trackThresh = 0.5f, float matchThresh = 0.8f);
  ~BYTETracker();

  std::vector<TrackObject> update(const std::vector<Detection> &detections);

private:
  struct STrack;

  static float iou(const cv::Rect_<float> &a, const cv::Rect_<float> &b);

  static std::vector<std::vector<float>>
  iouDistance(const std::vector<std::shared_ptr<STrack>> &aTracks,
             const std::vector<std::shared_ptr<STrack>> &bTracks);

  static void linearAssignment(const std::vector<std::vector<float>> &cost,
                               float thresh,
                               std::vector<std::pair<int, int>> &matches,
                               std::vector<int> &unmatchedA,
                               std::vector<int> &unmatchedB);

  int frameId_ = 0;
  int trackIdCounter_ = 0;
  int maxTimeLost_;

  float trackThresh_;
  float matchThresh_;

  std::vector<std::shared_ptr<STrack>> trackedStracks_;
  std::vector<std::shared_ptr<STrack>> lostStracks_;
};
