#pragma once

#include "barcode_data.h"
#include "bytetrack.h"
#include "upload_worker.h"
#include "yolov11.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <opencv2/opencv.hpp>

int findBestDetectionIdx(const TrackObject &track,
                          const std::vector<Detection> &dets,
                          float *outIoU = nullptr);

struct TrackEval {
  std::string barcodeText;
  std::string matchStatus;
};

TrackEval evalTrack(const TrackObject &track,
                     const std::vector<Detection> &dets,
                     const std::vector<std::string> &detLabels,
                     std::unordered_map<int, std::string> &barcodeCache,
                     const BarcodeDataMap &barcodeDataMap,
                     int cameraId = -1);

void enqueueMatchedBarcodes(
    int cameraId,
    const std::vector<TrackObject> &tracks,
    const std::vector<Detection> &dets,
    const std::vector<cv::Mat> &crops,
    const std::unordered_map<int, std::string> &barcodeCache,
    const BarcodeDataMap &barcodeDataMap,
    UploadManager &uploadMgr);

void cleanStaleCaches(int cameraId,
                      const std::vector<TrackObject> &tracks,
                      std::unordered_map<int, std::string> &barcodeCache,
                      UploadManager &uploadMgr);
