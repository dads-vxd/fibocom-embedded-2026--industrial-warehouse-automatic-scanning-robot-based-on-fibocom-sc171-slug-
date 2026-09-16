#include "upload_worker.h"
#include "ws_client.h"

#include <cstdint>
#include <iostream>

void UploadManager::start(const std::string &wsUrl) {
  wsUrl_ = wsUrl;
  running_ = true;
  thread_ = std::thread(&UploadManager::workerLoop, this);
}

void UploadManager::stop() {
  running_ = false;
  cv_.notify_one();
  if (thread_.joinable())
    thread_.join();
}

void UploadManager::enqueue(UploadItem item) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    pending_.push_back(std::move(item));
  }
  cv_.notify_one();
}

int64_t UploadManager::makeKey(int cameraId, int trackId) {
  return (static_cast<int64_t>(cameraId) << 32) | static_cast<uint32_t>(trackId);
}

bool UploadManager::isUploaded(int cameraId, int trackId) const {
  std::lock_guard<std::mutex> lock(mtx_);
  return uploaded_.count(makeKey(cameraId, trackId)) > 0;
}

bool UploadManager::isDataUploaded(int cameraId,
                                   const std::string &barcodeData) const {
  std::lock_guard<std::mutex> lock(mtx_);
  return uploadedData_.count(makeDataKey(cameraId, barcodeData)) > 0;
}

std::string UploadManager::makeDataKey(int cameraId,
                                        const std::string &data) {
  return std::to_string(cameraId) + "|" + data;
}

void UploadManager::cleanupStale(int cameraId,
                                  const std::vector<int> &activeIds) {
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto it = uploaded_.begin(); it != uploaded_.end();) {
    int cam = static_cast<int>(*it >> 32);
    if (cam != cameraId) {
      ++it;
      continue;
    }
    int tid = static_cast<int>(static_cast<uint32_t>(*it & 0xFFFFFFFF));
    if (std::find(activeIds.begin(), activeIds.end(), tid) == activeIds.end())
      it = uploaded_.erase(it);
    else
      ++it;
  }
}

void UploadManager::workerLoop() {
  while (running_) {
    std::vector<UploadItem> items;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait_for(lock, std::chrono::seconds(2),
                   [this] { return !pending_.empty() || !running_; });
      if (!running_ && pending_.empty())
        break;
      items = std::move(pending_);
      pending_.clear();
    }

    if (items.empty())
      continue;

    WsClient ws;
    ws.setLogCallback([](const std::string &msg) {
      std::cout << "[WS] " << msg << std::endl;
    });

    if (!ws.connect(wsUrl_)) {
      std::cerr << "[Upload] WS connect failed, retrying later" << std::endl;
      std::lock_guard<std::mutex> lock(mtx_);
      for (auto &it : items)
        pending_.push_back(std::move(it));
      continue;
    }

    for (auto &item : items) {
      {
        std::lock_guard<std::mutex> lock(mtx_);
        if (uploaded_.count(makeKey(item.cameraId, item.trackId))) {
          std::cout << "[Upload] Cam" << item.cameraId
                    << " Track " << item.trackId
                    << " already uploaded, skip" << std::endl;
          continue;
        }
      }

      nlohmann::json req;
      req["type"] = "upload_barcode";
      req["camera_id"] = item.cameraId;
      req["category_id"] = item.categoryId;
      req["data"] = item.barcodeData;
      if (!item.imageBase64.empty())
        req["image"] = item.imageBase64;
      req["fields"] = item.fields;

      std::cout << "[Upload] Cam" << item.cameraId
                << " Track " << item.trackId << ": "
                << item.barcodeData << std::endl;

      ws.sendText(req.dump());
      std::string respStr = ws.recvText();

      bool success = false;
      if (!respStr.empty()) {
        try {
          auto resp = nlohmann::json::parse(respStr);
          std::cout << "[Upload] Response: " << resp.dump(2) << std::endl;
          if (resp.contains("success") && resp["success"].get<bool>())
            success = true;
          else if (!resp.contains("error"))
            success = true;
        } catch (...) {
          success = true;
        }
      } else {
        success = true;
      }

      if (success) {
        std::lock_guard<std::mutex> lock(mtx_);
        uploaded_.insert(makeKey(item.cameraId, item.trackId));
        uploadedData_.insert(makeDataKey(item.cameraId, item.barcodeData));
        std::cout << "[Upload] Cam" << item.cameraId
                  << " Track " << item.trackId << " uploaded OK"
                  << std::endl;
      }
    }

    ws.disconnect();
  }
}
