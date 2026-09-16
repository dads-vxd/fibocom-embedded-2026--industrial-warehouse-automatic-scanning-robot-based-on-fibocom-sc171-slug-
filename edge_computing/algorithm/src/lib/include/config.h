#pragma once

#include "image_preprocess.h"

#include <string>
#include <vector>

struct CameraConfig {
	std::string deviceId = "/dev/video0";
  bool enabled = true;
  PreprocessConfig preprocess;
};

struct VideoRecordConfig {
  bool enabled = true;
  double fps = 5.0;
  int segmentSeconds = 30;
  int maxWidth = 1280;
  int maxHeight = 720;
};

struct AppConfig {
  std::string modelPath = "./models/best.onnx";
  std::string wsUrl;
  std::string controlWsUrl;
  std::string videoUploadUrl;
  double blurThreshold = 100.0;
  bool serialEnabled = false;
  std::string serialPort = "/dev/ttyUSB0";
  int serialBaudRate = 9600;
  PreprocessConfig preprocess;
  VideoRecordConfig videoRecord;
  std::vector<CameraConfig> cameras;
};

AppConfig loadAppConfig(const std::string &configPath);
