#pragma once

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct Detection {
  cv::RotatedRect rbox;
  float confidence;
  int classId;
};

class YOLOv11Detector {
public:
  YOLOv11Detector(const std::string &modelPath, int inputSize = 1280,
                  float confThreshold = 0.25f, float nmsThreshold = 0.45f);

  std::vector<Detection> detect(const cv::Mat &image);

  void drawDetections(cv::Mat &image,
                       const std::vector<Detection> &detections,
                       const std::vector<std::string> &labels = {});

  static std::vector<cv::Mat>
  cropDetections(const cv::Mat &image,
                 const std::vector<Detection> &detections);

private:
  Ort::Env env_{nullptr};
  Ort::Session session_{nullptr};
  std::string inputName_;
  std::string outputName_;
  int inputSize_;
  int numClasses_ = 0;
  float confThreshold_;
  float nmsThreshold_;

  void letterbox(const cv::Mat &image, cv::Mat &out, int &newW, int &newH,
                 int &padX, int &padY);
  std::vector<float> preprocess(const cv::Mat &letterboxed);
  void nms(std::vector<Detection> &detections);
  static float computeRotatedIoU(const cv::RotatedRect &a,
                                 const cv::RotatedRect &b);
};
