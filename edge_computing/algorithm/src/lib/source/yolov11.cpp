#include "yolov11.h"

#include "iou_utils.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

YOLOv11Detector::YOLOv11Detector(const std::string &modelPath, int inputSize,
                                 float confThreshold, float nmsThreshold)
    : inputSize_(inputSize), confThreshold_(confThreshold),
      nmsThreshold_(nmsThreshold) {
  env_ = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "YOLOv11");

  Ort::SessionOptions sessionOpts;
  sessionOpts.SetIntraOpNumThreads(4);
  sessionOpts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

  session_ = Ort::Session(env_, modelPath.c_str(), sessionOpts);

  Ort::AllocatorWithDefaultOptions allocator;
  inputName_ = session_.GetInputNameAllocated(0, allocator).get();
  outputName_ = session_.GetOutputNameAllocated(0, allocator).get();

  auto outputTypeInfo = session_.GetOutputTypeInfo(0);
  auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
  auto outputShape = outputTensorInfo.GetShape();
  // OBB output: [1, 4+num_classes+1, N]  (+1 for angle)
  if (outputShape.size() == 3) {
    numClasses_ = static_cast<int>(outputShape[1]) - 5;
  }

  std::cout << "Model loaded: " << modelPath << std::endl;
  std::cout << "Input:  " << inputName_ << std::endl;
  std::cout << "Output: " << outputName_ << std::endl;
  std::cout << "Classes: " << numClasses_ << std::endl;
}

std::vector<Detection> YOLOv11Detector::detect(const cv::Mat &image) {
  cv::Mat letterboxed;
  int newW, newH, padX, padY;
  letterbox(image, letterboxed, newW, newH, padX, padY);

  float realScaleX = static_cast<float>(image.cols) / newW;
  float realScaleY = static_cast<float>(image.rows) / newH;

  std::vector<float> inputTensorValues = preprocess(letterboxed);

  std::array<int64_t, 4> inputShape = {1, 3, inputSize_, inputSize_};
  Ort::MemoryInfo memInfo =
      Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
      memInfo, inputTensorValues.data(), inputTensorValues.size(),
      inputShape.data(), inputShape.size());

  const char *inputNames[] = {inputName_.c_str()};
  const char *outputNames[] = {outputName_.c_str()};

  auto start = std::chrono::high_resolution_clock::now();
  auto outputTensors = session_.Run(Ort::RunOptions{nullptr}, inputNames,
                                    &inputTensor, 1, outputNames, 1);
  auto end = std::chrono::high_resolution_clock::now();
  float inferMs = std::chrono::duration<float, std::milli>(end - start).count();
  // std::cout << "Inference: " << inferMs << " ms" << std::endl;

  auto &outputTensor = outputTensors[0];
  auto outputInfo = outputTensor.GetTensorTypeAndShapeInfo();
  auto outputShape = outputInfo.GetShape();

  float *outputData = outputTensor.GetTensorMutableData<float>();
  int channels = static_cast<int>(outputShape[1]);
  int numPredictions = static_cast<int>(outputShape[2]);

  int numCls = channels - 5;
  if (numCls <= 0)
    numCls = 1;
  int angleIdx = 4 + numCls;

  std::vector<Detection> detections;
  for (int i = 0; i < numPredictions; ++i) {
    float cx = outputData[0 * numPredictions + i];
    float cy = outputData[1 * numPredictions + i];
    float w = outputData[2 * numPredictions + i];
    float h = outputData[3 * numPredictions + i];

    float maxConf = 0.f;
    int bestCls = 0;
    for (int c = 0; c < numCls; ++c) {
      float conf = outputData[(4 + c) * numPredictions + i];
      if (conf > maxConf) {
        maxConf = conf;
        bestCls = c;
      }
    }

    if (maxConf < confThreshold_)
      continue;

    float angle = outputData[angleIdx * numPredictions + i];
    float angleDeg = angle * 180.0f / static_cast<float>(CV_PI);

    float rx = (cx - static_cast<float>(padX)) * realScaleX;
    float ry = (cy - static_cast<float>(padY)) * realScaleY;
    float rw = w * realScaleX;
    float rh = h * realScaleY;

    Detection det;
    det.rbox =
        cv::RotatedRect(cv::Point2f(rx, ry), cv::Size2f(rw, rh), angleDeg);
    det.confidence = maxConf;
    det.classId = bestCls;
    detections.push_back(det);
  }

  nms(detections);
  return detections;
}

std::vector<cv::Mat>
YOLOv11Detector::cropDetections(const cv::Mat &image,
                                const std::vector<Detection> &detections) {
  std::vector<cv::Mat> crops;
  crops.reserve(detections.size());

  for (const auto &det : detections) {
    cv::Mat rotMat =
        cv::getRotationMatrix2D(det.rbox.center, det.rbox.angle, 1.0);
    cv::Rect bbox = det.rbox.boundingRect();

    rotMat.at<double>(0, 2) -= bbox.x;
    rotMat.at<double>(1, 2) -= bbox.y;

    cv::Mat rotated;
    cv::warpAffine(image, rotated, rotMat, bbox.size(), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    float w = det.rbox.size.width;
    float h = det.rbox.size.height;
    if (w < h)
      std::swap(w, h);

    int cropW = static_cast<int>(std::round(w));
    int cropH = static_cast<int>(std::round(h));
    int cx = rotated.cols / 2;
    int cy = rotated.rows / 2;

    cv::Rect cropRect(cx - cropW / 2, cy - cropH / 2, cropW, cropH);
    cropRect &= cv::Rect(0, 0, rotated.cols, rotated.rows);

    crops.push_back(rotated(cropRect).clone());
  }
  return crops;
}

void YOLOv11Detector::drawDetections(cv::Mat &image,
                                     const std::vector<Detection> &detections,
                                     const std::vector<std::string> &labels) {
  for (size_t i = 0; i < detections.size(); ++i) {
    const auto &det = detections[i];
    cv::Point2f vertices[4];
    det.rbox.points(vertices);

    cv::Scalar boxColor(0, 255, 0);
    if (i < labels.size() && labels[i].find("BLUR") != std::string::npos)
      boxColor = cv::Scalar(0, 165, 255);

    for (int j = 0; j < 4; ++j) {
      cv::line(image, vertices[j], vertices[(j + 1) % 4], boxColor, 2);
    }

    std::string label = "cls" + std::to_string(det.classId) + " " +
                        cv::format("%.2f", det.confidence);
    if (i < labels.size())
      label += " " + labels[i];

    int baseline = 0;
    cv::Size labelSize =
        cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

    cv::Point2f tl = vertices[1];
    int textX = static_cast<int>(tl.x);
    int textY = static_cast<int>(tl.y) - 4;
    textY = std::max(textY, labelSize.height);

    cv::rectangle(image, cv::Point(textX, textY - labelSize.height - 2),
                  cv::Point(textX + labelSize.width, textY + 2), boxColor, -1);
    cv::putText(image, label, cv::Point(textX, textY), cv::FONT_HERSHEY_SIMPLEX,
                0.5, cv::Scalar(0, 0, 0), 1);
  }
}

void YOLOv11Detector::letterbox(const cv::Mat &image, cv::Mat &out, int &newW,
                                int &newH, int &padX, int &padY) {
  int imgW = image.cols;
  int imgH = image.rows;
  float scale = std::min(static_cast<float>(inputSize_) / imgW,
                         static_cast<float>(inputSize_) / imgH);

  newW = static_cast<int>(imgW * scale);
  newH = static_cast<int>(imgH * scale);
  padX = (inputSize_ - newW) / 2;
  padY = (inputSize_ - newH) / 2;

  cv::Mat resized;
  cv::resize(image, resized, cv::Size(newW, newH));

  out = cv::Mat(inputSize_, inputSize_, CV_8UC3, cv::Scalar(114, 114, 114));
  resized.copyTo(out(cv::Rect(padX, padY, newW, newH)));
}

std::vector<float> YOLOv11Detector::preprocess(const cv::Mat &letterboxed) {
  cv::Mat rgb;
  cv::cvtColor(letterboxed, rgb, cv::COLOR_BGR2RGB);

  cv::Mat floatImg;
  rgb.convertTo(floatImg, CV_32F, 1.0 / 255.0);

  std::vector<float> tensor(3 * inputSize_ * inputSize_);
  std::vector<cv::Mat> channels(3);
  cv::split(floatImg, channels);

  int channelSize = inputSize_ * inputSize_;
  for (int c = 0; c < 3; ++c) {
    std::memcpy(tensor.data() + c * channelSize, channels[c].data,
                channelSize * sizeof(float));
  }
  return tensor;
}

void YOLOv11Detector::nms(std::vector<Detection> &detections) {
  std::sort(detections.begin(), detections.end(),
            [](const Detection &a, const Detection &b) {
              return a.confidence > b.confidence;
            });

  std::vector<bool> suppressed(detections.size(), false);
  for (size_t i = 0; i < detections.size(); ++i) {
    if (suppressed[i])
      continue;
    for (size_t j = i + 1; j < detections.size(); ++j) {
      if (suppressed[j])
        continue;
      float iou = computeRotatedIoU(detections[i].rbox, detections[j].rbox);
      if (iou > nmsThreshold_) {
        suppressed[j] = true;
      }
    }
  }

  std::vector<Detection> result;
  for (size_t i = 0; i < detections.size(); ++i) {
    if (!suppressed[i])
      result.push_back(detections[i]);
  }
  detections = std::move(result);
}

float YOLOv11Detector::computeRotatedIoU(const cv::RotatedRect &a,
                                         const cv::RotatedRect &b) {
  return ::computeRotatedIoU(a, b);
}
