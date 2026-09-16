#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class VideoUploader {
public:
  void start(const std::string &uploadUrl);
  void stop();
  void enqueue(const std::string &filePath);

private:
  void workerLoop();
  bool uploadFile(const std::string &filePath);

  std::string uploadUrl_;
  std::atomic<bool> running_{false};
  std::thread thread_;

  std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<std::string> pending_;
};
