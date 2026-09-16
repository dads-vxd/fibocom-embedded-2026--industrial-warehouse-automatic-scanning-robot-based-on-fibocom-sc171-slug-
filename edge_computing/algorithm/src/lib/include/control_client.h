#pragma once

#include "ws_client.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

// Persistent WebSocket link to the backend control channel (/ws/device-control).
//
//   - Incoming {"command":"..."} messages are dispatched to a callback that
//     maps the command to a serial write.
//   - sendLog() pushes {"log":"..."} up to the backend so it can be forwarded
//     to the browser.
//
// All WsClient access is serialized with an internal mutex because OpenSSL is
// not safe for concurrent read/write on the same SSL object. The receive loop
// polls with a short timeout so that sendLog() never blocks for long.
class ControlClient {
public:
  using CommandCallback = std::function<void(const std::string &command)>;

  void setCommandCallback(CommandCallback cb) { commandCb_ = std::move(cb); }

  void start(const std::string &url);
  void stop();

  void sendLog(const std::string &text);
  bool isConnected() const;

private:
  void run();
  void handleIncoming(const std::string &msg);

  std::string url_;
  std::atomic<bool> running_{false};
  std::thread thread_;

  mutable std::mutex mtx_;
  WsClient ws_;
  CommandCallback commandCb_;
};
