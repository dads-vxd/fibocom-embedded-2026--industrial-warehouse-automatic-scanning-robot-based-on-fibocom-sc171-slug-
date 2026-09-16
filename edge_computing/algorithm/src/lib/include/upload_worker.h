#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

struct UploadItem {
  int cameraId = 0;
  int trackId;
  std::string barcodeData;
  int64_t categoryId;
  nlohmann::json fields;
  std::string imageBase64;
};

class UploadManager {
public:
  void start(const std::string &wsUrl);
  void stop();

  void enqueue(UploadItem item);
  bool isUploaded(int cameraId, int trackId) const;
  bool isDataUploaded(int cameraId, const std::string &barcodeData) const;

  void cleanupStale(int cameraId, const std::vector<int> &activeIds);

private:
  void workerLoop();
  static int64_t makeKey(int cameraId, int trackId);
  static std::string makeDataKey(int cameraId, const std::string &data);

  std::string wsUrl_;
  std::atomic<bool> running_{false};
  std::thread thread_;

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<UploadItem> pending_;
  std::unordered_set<int64_t> uploaded_;
  std::unordered_set<std::string> uploadedData_;
};
