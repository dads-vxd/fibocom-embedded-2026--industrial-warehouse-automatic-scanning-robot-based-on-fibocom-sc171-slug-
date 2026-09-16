#include "video_recorder.h"

#include <ctime>
#include <filesystem>
#include <iostream>

VideoRecorder::VideoRecorder(int cameraId, const VideoRecorderConfig &cfg)
    : cameraId_(cameraId), cfg_(cfg) {}

std::string VideoRecorder::timestamp() {
  time_t now = time(nullptr);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
  return std::string(buf);
}

cv::Mat VideoRecorder::resizeFrame(const cv::Mat &frame) {
  if (frameScale_ >= 1.0)
    return frame;
  cv::Mat resized;
  cv::resize(frame, resized, cv::Size(), frameScale_, frameScale_, cv::INTER_LINEAR);
  return resized;
}

void VideoRecorder::startSegment(const cv::Mat &frame) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (recording_)
    return;

  std::filesystem::create_directories("./videos");

  currentPath_ = "./videos/" + std::to_string(cameraId_) + "_" + timestamp() + ".mp4";

  int fw = frame.cols;
  int fh = frame.rows;
  if (fw > cfg_.maxWidth || fh > cfg_.maxHeight) {
    frameScale_ = std::min(static_cast<double>(cfg_.maxWidth) / fw,
                           static_cast<double>(cfg_.maxHeight) / fh);
  } else {
    frameScale_ = 1.0;
  }

  cv::Mat firstFrame = resizeFrame(frame);
  cv::Size frameSize(firstFrame.cols, firstFrame.rows);
  int fourcc = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
  writer_.open(currentPath_, fourcc, cfg_.fps, frameSize);
  if (!writer_.isOpened()) {
    fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    writer_.open(currentPath_, fourcc, cfg_.fps, frameSize);
  }
  if (!writer_.isOpened()) {
    std::cerr << "[Video] Failed to open writer: " << currentPath_ << std::endl;
    return;
  }

  recording_ = true;
  frameCount_ = 0;
  segmentStartTime_ = std::chrono::steady_clock::now();
  lastFrameTime_ = segmentStartTime_;
  std::cout << "[Video] Started: " << currentPath_
            << " (" << frameSize.width << "x" << frameSize.height
            << " @" << cfg_.fps << "fps)"
            << std::endl;
}

void VideoRecorder::finalizeSegment() {
  std::lock_guard<std::mutex> lock(mtx_);
  if (writer_.isOpened()) {
    writer_.release();
    recording_ = false;
    std::cout << "[Video] Saved: " << currentPath_ << " ("
              << frameCount_ << " frames)" << std::endl;
    if (onSegmentDone_)
      onSegmentDone_(currentPath_);
  }
}

void VideoRecorder::processFrame(const cv::Mat &frame) {
  if (!recording_) {
    startSegment(frame);
  }
  if (!recording_)
    return;

  auto now = std::chrono::steady_clock::now();
  double sinceLast = std::chrono::duration<double>(now - lastFrameTime_).count();
  double sinceStart = std::chrono::duration<double>(now - segmentStartTime_).count();

  if (sinceLast >= 1.0 / cfg_.fps || frameCount_ == 0) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (writer_.isOpened()) {
      writer_.write(resizeFrame(frame));
    }
    lastFrameTime_ = now;
    frameCount_++;
  }

  if (sinceStart >= cfg_.segmentSeconds ||
      frameCount_ >= static_cast<int>(cfg_.fps * cfg_.segmentSeconds)) {
    finalizeSegment();
  }
}

void VideoRecorder::stop() {
  finalizeSegment();
}
