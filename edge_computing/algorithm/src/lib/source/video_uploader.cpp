#include "video_uploader.h"

#include <curl/curl.h>
#include <iostream>
#include <sys/stat.h>
#include <unordered_map>

static size_t discardBody(void *, size_t size, size_t nmemb, void *) {
  return size * nmemb;
}

void VideoUploader::start(const std::string &uploadUrl) {
  uploadUrl_ = uploadUrl;
  running_ = true;
  thread_ = std::thread(&VideoUploader::workerLoop, this);
}

void VideoUploader::stop() {
  running_ = false;
  cv_.notify_one();
  if (thread_.joinable())
    thread_.join();
}

void VideoUploader::enqueue(const std::string &filePath) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    pending_.push_back(filePath);
  }
  cv_.notify_one();
}

bool VideoUploader::uploadFile(const std::string &filePath) {
  struct stat st;
  if (stat(filePath.c_str(), &st) != 0)
    return false;

  CURL *curl = curl_easy_init();
  if (!curl)
    return false;

  curl_mime *form = curl_mime_init(curl);
  curl_mimepart *field = curl_mime_addpart(form);
  curl_mime_name(field, "video");
  curl_mime_filedata(field, filePath.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, uploadUrl_.c_str());
  curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardBody);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

  CURLcode res = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  curl_mime_free(form);
  curl_easy_cleanup(curl);

  bool ok = (res == CURLE_OK && httpCode >= 200 && httpCode < 300);
  if (ok)
    std::cout << "[Upload] Video uploaded: " << filePath
              << " HTTP " << httpCode << std::endl;
  else
    std::cerr << "[Upload] Video upload failed: " << filePath
              << " HTTP " << httpCode << " err=" << curl_easy_strerror(res)
              << std::endl;
  return ok;
}

void VideoUploader::workerLoop() {
  std::unordered_map<std::string, int> retryCount;
  while (running_) {
    std::string file;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait_for(lock, std::chrono::seconds(5),
                   [this] { return !pending_.empty() || !running_; });
      if (!running_ && pending_.empty())
        break;
      if (pending_.empty())
        continue;
      file = std::move(pending_.front());
      pending_.erase(pending_.begin());
    }

    if (!uploadFile(file)) {
      int &rc = retryCount[file];
      rc++;
      if (rc <= 3) {
        std::lock_guard<std::mutex> lock(mtx_);
        pending_.push_back(std::move(file));
      } else {
        std::cerr << "[Upload] Giving up on: " << file << std::endl;
        retryCount.erase(file);
      }
    } else {
      retryCount.erase(file);
    }
  }
}
