#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <opencv2/opencv.hpp>

struct VideoRecorderConfig {
  double fps = 3.0;
  int segmentSeconds = 30;
  int maxWidth = 1280;
  int maxHeight = 720;
};

class VideoRecorder {
public:
  using SegmentDoneCb = std::function<void(const std::string &filePath)>;

  VideoRecorder(int cameraId = 0, const VideoRecorderConfig &cfg = {});

  void processFrame(const cv::Mat &frame);
  void stop();

  void setOnSegmentDone(SegmentDoneCb cb) { onSegmentDone_ = std::move(cb); }
  bool isRecording() const { return recording_; }

private:
  void startSegment(const cv::Mat &frame);
  void finalizeSegment();
  static std::string timestamp();

  cv::Mat resizeFrame(const cv::Mat &frame);

  int cameraId_;
  VideoRecorderConfig cfg_;

  std::mutex mtx_;
  cv::VideoWriter writer_;
  std::string currentPath_;
  bool recording_ = false;
  int frameCount_ = 0;
  double frameScale_ = 1.0;
  std::chrono::steady_clock::time_point lastFrameTime_;
  std::chrono::steady_clock::time_point segmentStartTime_;
  SegmentDoneCb onSegmentDone_;
};
