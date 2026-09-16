#pragma once

#include <opencv2/opencv.hpp>

float computeIoU(const cv::Rect_<float> &a, const cv::Rect_<float> &b);

float computeRotatedIoU(const cv::RotatedRect &a, const cv::RotatedRect &b);
