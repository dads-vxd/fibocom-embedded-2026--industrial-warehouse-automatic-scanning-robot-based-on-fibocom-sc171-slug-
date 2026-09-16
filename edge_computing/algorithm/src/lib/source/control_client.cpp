#include "control_client.h"

#include <chrono>
#include <iostream>

#include <nlohmann/json.hpp>

void ControlClient::start(const std::string &url) {
  url_ = url;
  running_ = true;
  thread_ = std::thread(&ControlClient::run, this);
}

void ControlClient::stop() {
  running_ = false;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    ws_.disconnect();
  }
  if (thread_.joinable())
    thread_.join();
}

bool ControlClient::isConnected() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return ws_.isConnected();
}

void ControlClient::sendLog(const std::string &text) {
  nlohmann::json j;
  j["log"] = text;
  const std::string payload = j.dump();

  std::lock_guard<std::mutex> lock(mtx_);
  if (!ws_.isConnected())
    return;
  if (!ws_.sendText(payload))
    std::cerr << "[Control] sendLog failed: " << text << std::endl;
}

void ControlClient::run() {
  while (running_) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      ws_.disconnect();
      ws_.setLogCallback(
          [](const std::string &m) { std::cout << "[Control] " << m << std::endl; });
      if (!ws_.connect(url_)) {
        std::cerr << "[Control] connect failed, retrying in 2s" << std::endl;
      }
    }

    if (!isConnected()) {
      for (int i = 0; i < 20 && running_; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    while (running_ && isConnected()) {
      std::string msg;
      {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!running_ || !ws_.isConnected())
          break;
        msg = ws_.recvText(200);
      }
      if (!msg.empty() && isConnected())
        handleIncoming(msg);
    }
  }
}

void ControlClient::handleIncoming(const std::string &msg) {
  try {
    auto j = nlohmann::json::parse(msg);
    if (j.contains("command") && j["command"].is_string()) {
      std::string cmd = j["command"].get<std::string>();
      std::cout << "[Control] command: " << cmd << std::endl;
      if (commandCb_)
        commandCb_(cmd);
    }
  } catch (const std::exception &e) {
    std::cerr << "[Control] parse error: " << e.what() << std::endl;
  }
}
