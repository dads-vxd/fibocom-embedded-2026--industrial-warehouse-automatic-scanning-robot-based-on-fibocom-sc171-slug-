#pragma once

#include <functional>
#include <memory>
#include <string>

class WsClient {
public:
  using LogCallback = std::function<void(const std::string &)>;

  WsClient();
  ~WsClient();

  WsClient(const WsClient &) = delete;
  WsClient &operator=(const WsClient &) = delete;

  bool connect(const std::string &url);
  void disconnect();

  bool isConnected() const;

  bool sendText(const std::string &message);
  // Receive one text frame. When timeoutMs < 0 (default) this blocks until a
  // frame arrives or the connection breaks. When timeoutMs >= 0 it waits at
  // most that long; on timeout it returns "" with the connection still open
  // (use isConnected() to distinguish a timeout from a real disconnect).
  std::string recvText(int timeoutMs = -1);

  void setLogCallback(LogCallback cb);

private:
  struct State;
  std::unique_ptr<State> s_;
};
