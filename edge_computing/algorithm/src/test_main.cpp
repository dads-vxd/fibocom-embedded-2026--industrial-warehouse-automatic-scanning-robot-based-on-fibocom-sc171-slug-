#include "barcode_data.h"
#include "bytetrack.h"
#include "config.h"
#include "drawing.h"
#include "pipeline.h"
#include "qr_scanner.h"
#include "serial_sender.h"
#include "upload_worker.h"
#include "video_recorder.h"
#include "yolov11.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <thread>
#include <opencv2/opencv.hpp>
#include <unordered_map>

struct CameraPipeline {
  int id;
  CameraConfig cfg;
  cv::VideoCapture cap;
  BYTETracker tracker;
  QRScanner qrScanner;
  VideoRecorder recorder;
  std::unordered_map<int, std::string> barcodeCache;

  std::vector<int> lastTrackIds;
  std::vector<int> curTrackIds;
  int stableFrames = 0;
  int framesSinceLastSend = 0;
  std::chrono::steady_clock::time_point delayUntil =
      std::chrono::steady_clock::now();
  bool inDelay = false;

  CameraPipeline(int id_, const CameraConfig &cc,
                 const VideoRecordConfig &vrc)
      : id(id_), cfg(cc), tracker(30, 30, 0.25f, 0.8f),
        qrScanner("./models/detect.prototxt", "./models/detect.caffemodel",
                  "./models/sr.prototxt", "./models/sr.caffemodel"),
        recorder(id, VideoRecorderConfig{vrc.fps, vrc.segmentSeconds,
                                           vrc.maxWidth, vrc.maxHeight}) {

     cap.open(cfg.deviceId);
    if (!cap.isOpened())
      return;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 4096);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 2160);
  }

  bool isOpen() const { return cap.isOpened(); }

  bool read(cv::Mat &frame) {
    cap >> frame;
    return !frame.empty();
  }
};

int main(int, char **) {
  AppConfig cfg = loadAppConfig("./config.json");
  auto barcodeDataMap = loadBarcodeData("./data.json");

  YOLOv11Detector detector(cfg.modelPath);

  UploadManager uploadMgr;

  SerialSender serialSender;
  if (cfg.serialEnabled) {
    serialSender.start(cfg.serialPort, cfg.serialBaudRate);
    serialSender.send("1");
  }

  cfg.videoRecord.maxWidth = 4096;
  cfg.videoRecord.maxHeight = 2160;

  std::deque<CameraPipeline> cams;
  for (size_t i = 0; i < cfg.cameras.size(); ++i) {
    if (!cfg.cameras[i].enabled)
      continue;
    cams.emplace_back(static_cast<int>(i), cfg.cameras[i],
                      cfg.videoRecord);
    if (!cams.back().isOpen())
      cams.pop_back();
  }

  if (cams.empty()) {
    std::cerr << "No camera available, exiting" << std::endl;
    return 1;
  }

  std::cout << "Running with " << cams.size() << " camera(s)" << std::endl;

  cv::Mat frame;
  size_t camIdx = 0;
  std::chrono::steady_clock::time_point globalSerialCooldown =
      std::chrono::steady_clock::now();
  while (true) {
    auto &cam = cams[camIdx];

    if (!cam.read(frame)) {
      std::cerr << "[Cam" << cam.id << "] Read failed, skipping"
                << std::endl;
      camIdx = (camIdx + 1) % cams.size();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    auto dets = detector.detect(frame);
    auto tracks = cam.tracker.update(dets);
    auto crops = YOLOv11Detector::cropDetections(frame, dets);

    std::vector<std::string> detLabels(dets.size());
    for (size_t i = 0; i < crops.size(); ++i) {
      cv::Mat gray;
      cv::cvtColor(crops[i], gray, cv::COLOR_BGR2GRAY);
      if (isBlurry(gray, cfg.blurThreshold)) {
        detLabels[i] += "BLUR";
      } else {
        auto results = cam.qrScanner.scan(crops[i]);
        if (!results.empty()) {
          detLabels[i] += "QR:" + results[0].data;
          for (size_t k = 1; k < results.size(); ++k)
            detLabels[i] += "|" + results[k].data;
        }
      }
    }

    detector.drawDetections(frame, dets, detLabels);

    for (const auto &t : tracks) {
      cv::Scalar color = trackIdToColor(t.trackId);
      cv::Point2f vertices[4];
      t.rbox.points(vertices);
      for (int j = 0; j < 4; ++j)
        cv::line(frame, vertices[j], vertices[(j + 1) % 4], color, 2);

      cv::Point2f bottomV = vertices[0];
      for (int j = 1; j < 4; ++j)
        if (vertices[j].y > bottomV.y)
          bottomV = vertices[j];

      auto eval =
          evalTrack(t, dets, detLabels, cam.barcodeCache, barcodeDataMap,
                    cam.id);

      TrackDisplayInfo info;
      info.barcodeText = eval.barcodeText;
      info.matchStatus = eval.matchStatus;
      info.isUploaded = uploadMgr.isUploaded(cam.id, t.trackId);
      drawTrackInfo(frame, bottomV, t.rbox.center, t.trackId, color, info);
    }

    cleanStaleCaches(cam.id, tracks, cam.barcodeCache, uploadMgr);

    if (cfg.serialEnabled) {
      std::vector<int> curIds;
      for (const auto &t : tracks)
        curIds.push_back(t.trackId);
      std::sort(curIds.begin(), curIds.end());

      bool changed = (curIds.size() != cam.lastTrackIds.size());
      if (!changed) {
        for (size_t i = 0; i < curIds.size(); ++i) {
          if (curIds[i] != cam.lastTrackIds[i]) {
            changed = true;
            break;
          }
        }
      }
      cam.lastTrackIds = curIds;
      cam.curTrackIds = curIds;

      cam.framesSinceLastSend++;

      if (cam.inDelay) {
        if (std::chrono::steady_clock::now() >= cam.delayUntil)
          cam.inDelay = false;
      } else {
        if (changed)
          cam.stableFrames = 0;
        else
          cam.stableFrames++;
      }
    }

    cam.recorder.processFrame(frame);

    std::string winName = "Camera-" + std::to_string(cam.id);

    if (cv::waitKey(1) == 27)
      break;

    camIdx = (camIdx + 1) % cams.size();

    if (cfg.serialEnabled) {
      bool sendNow = false;
      bool anyActive = false;
      for (auto &c : cams) {
        if (!c.curTrackIds.empty())
          anyActive = true;
        if (!c.inDelay && c.framesSinceLastSend >= 400 &&
            !c.curTrackIds.empty()) {
          sendNow = true;
          break;
        }
      }

      if (!sendNow && anyActive) {
        bool allReady = true;
        for (auto &c : cams) {
          if (c.inDelay) {
            allReady = false;
            break;
          }
          if (c.curTrackIds.empty()) {
            allReady = false;
            break;
          }
          if (c.stableFrames < 10) {
            allReady = false;
            break;
          }
          for (int tid : c.curTrackIds) {
            if (c.barcodeCache.find(tid) == c.barcodeCache.end()) {
              allReady = false;
              break;
            }
          }
          if (!allReady)
            break;
        }
        if (allReady)
          sendNow = true;
      }

      if (sendNow) {
        auto now = std::chrono::steady_clock::now();
        if (now >= globalSerialCooldown) {
          serialSender.send("2");
          globalSerialCooldown = now + std::chrono::seconds(5);
        }
        for (auto &c : cams) {
          c.delayUntil = now + std::chrono::seconds(5);
          c.inDelay = true;
          c.framesSinceLastSend = 0;
          c.stableFrames = 0;
        }
      }
    }
  }

  for (auto &cam : cams)
    cam.recorder.stop();
  serialSender.stop();
  return 0;
}
