#include "bytetrack.h"
#include "drawing.h"
#include "yolov11.h"

#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

static void drawDetectionsRaw(cv::Mat &image,
                              const std::vector<Detection> &dets) {
  for (size_t i = 0; i < dets.size(); ++i) {
    cv::Point2f v[4];
    dets[i].rbox.points(v);
    for (int j = 0; j < 4; ++j)
      cv::line(image, v[j], v[(j + 1) % 4], cv::Scalar(0, 255, 0), 1);
    std::string lbl = "det" + std::to_string(i) + " " +
                      cv::format("%.2f", dets[i].confidence);
    cv::putText(image, lbl, v[1], cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 255, 0), 1);
  }
}

int main(int argc, char *argv[]) {
  std::string modelPath = "./models/best.onnx";
  int deviceId = 0;
  bool useCamera = true;
  std::string videoPath;

  if (argc > 1) {
    useCamera = false;
    videoPath = argv[1];
  }

  YOLOv11Detector detector(modelPath);
  BYTETracker tracker(30, 30, 0.25f, 0.8f);

  cv::VideoCapture cap;
  if (useCamera) {
    cap.open(deviceId);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
  } else {
    cap.open(videoPath);
  }

  if (!cap.isOpened()) {
    std::cerr << "Cannot open video source" << std::endl;
    return 1;
  }

  cv::Mat frame;
  int frameId = 0;
  while (true) {
    cap >> frame;
    if (frame.empty())
      break;
    frameId++;

    auto t0 = std::chrono::high_resolution_clock::now();
    auto dets = detector.detect(frame);
    auto tracks = tracker.update(dets);
    auto t1 = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();

    drawDetectionsRaw(frame, dets);

    for (const auto &t : tracks) {
      cv::Scalar color = trackIdToColor(t.trackId);
      cv::Point2f vertices[4];
      t.rbox.points(vertices);
      for (int j = 0; j < 4; ++j)
        cv::line(frame, vertices[j], vertices[(j + 1) % 4], color, 3);

      cv::Point2f bottomV = vertices[0];
      for (int j = 1; j < 4; ++j)
        if (vertices[j].y > bottomV.y)
          bottomV = vertices[j];

      TrackDisplayInfo info;
      info.barcodeText = "null";
      drawTrackInfo(frame, bottomV, t.rbox.center, t.trackId, color, info);
    }

    std::cout << "Frame " << frameId
              << " | Dets: " << dets.size()
              << " | Tracks: " << tracks.size()
              << " | ms: " << ms << std::endl;

    cv::imshow("track", frame);
    if (cv::waitKey(1) == 27)
      break;
  }

  return 0;
}
