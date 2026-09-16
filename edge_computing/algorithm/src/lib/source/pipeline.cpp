#include "pipeline.h"
#include "base64.h"
#include "iou_utils.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>

int findBestDetectionIdx(const TrackObject &track,
                          const std::vector<Detection> &dets,
                          float *outIoU) {
  cv::Rect tBr = track.rbox.boundingRect();
  float bestIoU = 0.f;
  int bestIdx = -1;
  for (size_t di = 0; di < dets.size(); ++di) {
    cv::Rect dBr = dets[di].rbox.boundingRect();
    cv::Rect_<float> rA(static_cast<float>(tBr.x), static_cast<float>(tBr.y),
                         static_cast<float>(tBr.width),
                         static_cast<float>(tBr.height));
    cv::Rect_<float> rB(static_cast<float>(dBr.x), static_cast<float>(dBr.y),
                         static_cast<float>(dBr.width),
                         static_cast<float>(dBr.height));
    float iou = computeIoU(rA, rB);
    if (iou > bestIoU) {
      bestIoU = iou;
      bestIdx = static_cast<int>(di);
    }
  }
  if (outIoU)
    *outIoU = bestIoU;
  return bestIdx;
}

TrackEval evalTrack(const TrackObject &track,
                     const std::vector<Detection> &dets,
                     const std::vector<std::string> &detLabels,
                     std::unordered_map<int, std::string> &barcodeCache,
                     const BarcodeDataMap &barcodeDataMap,
                     int cameraId) {
  TrackEval eval;
  eval.barcodeText = "null";

  float bestIoU = 0.f;
  int bestIdx = findBestDetectionIdx(track, dets, &bestIoU);

  if (bestIdx >= 0 && bestIoU > 0.3f &&
      bestIdx < static_cast<int>(detLabels.size()) &&
      !detLabels[bestIdx].empty() && detLabels[bestIdx] != "BLUR") {
    barcodeCache[track.trackId] = detLabels[bestIdx];
  }

  auto bcIt = barcodeCache.find(track.trackId);
  if (bcIt != barcodeCache.end()) {
    eval.barcodeText = bcIt->second;
    std::string rawData = extractBarcodeData(eval.barcodeText);
    if (barcodeDataMap.count(rawData)) {
      eval.matchStatus = "MATCHED";
    } else {
      static std::set<std::string> seenUnmatched;
      if (seenUnmatched.insert(rawData).second) {
        std::cout << "[QR] Cam" << cameraId
                  << " track " << track.trackId
                  << " unmatched: [" << rawData
                  << "] len=" << rawData.size() << std::endl;
      }
    }
  }

  return eval;
}

void enqueueMatchedBarcodes(
    int cameraId,
    const std::vector<TrackObject> &tracks,
    const std::vector<Detection> &dets,
    const std::vector<cv::Mat> &crops,
    const std::unordered_map<int, std::string> &barcodeCache,
    const BarcodeDataMap &barcodeDataMap,
    UploadManager &uploadMgr) {
  for (const auto &t : tracks) {
    if (uploadMgr.isUploaded(cameraId, t.trackId))
      continue;
    auto bcIt = barcodeCache.find(t.trackId);
    if (bcIt == barcodeCache.end())
      continue;

    std::string rawData = extractBarcodeData(bcIt->second);
    auto matchIt = barcodeDataMap.find(rawData);
    if (matchIt == barcodeDataMap.end())
      continue;

    if (uploadMgr.isDataUploaded(cameraId, rawData))
      continue;

    int bestDetIdx = findBestDetectionIdx(t, dets);

    std::string imgB64;
    if (bestDetIdx >= 0 && bestDetIdx < static_cast<int>(crops.size())) {
      std::vector<uint8_t> buf;
      std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 95};
      cv::imencode(".jpg", crops[bestDetIdx], buf, params);
      imgB64 = "data:image/jpeg;base64," + base64Encode(buf);
    }

    UploadItem item;
    item.cameraId = cameraId;
    item.trackId = t.trackId;
    item.barcodeData = rawData;
    item.categoryId = matchIt->second.categoryId;
    item.fields = matchIt->second.fields;
    item.imageBase64 = imgB64;
    uploadMgr.enqueue(std::move(item));
  }
}

void cleanStaleCaches(int cameraId,
                      const std::vector<TrackObject> &tracks,
                      std::unordered_map<int, std::string> &barcodeCache,
                      UploadManager &uploadMgr) {
  std::vector<int> activeIds;
  for (const auto &t : tracks)
    activeIds.push_back(t.trackId);
  for (auto it = barcodeCache.begin(); it != barcodeCache.end();) {
    if (std::find(activeIds.begin(), activeIds.end(), it->first) ==
        activeIds.end())
      it = barcodeCache.erase(it);
    else
      ++it;
  }
  uploadMgr.cleanupStale(cameraId, activeIds);
}
