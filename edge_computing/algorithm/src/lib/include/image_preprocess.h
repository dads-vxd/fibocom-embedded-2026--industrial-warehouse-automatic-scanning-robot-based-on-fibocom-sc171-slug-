#pragma once

#include <opencv2/opencv.hpp>

struct PreprocessConfig {
  double scale = 2.0;
  bool enable_clahe = true;
  double clahe_clip_limit = 3.0;
  bool enable_sharpen = true;
  double sharpen_alpha = 1.5;
  bool enable_binary = true;
  int binary_block_size = 31;
  int binary_c = 10;
};

struct PreprocessedImage {
  cv::Mat bgr;
  cv::Mat gray;
  cv::Mat binary;
  cv::Mat sharpened_gray;
};

PreprocessedImage preprocessImage(const cv::Mat &crop,
                                  const PreprocessConfig &cfg);
