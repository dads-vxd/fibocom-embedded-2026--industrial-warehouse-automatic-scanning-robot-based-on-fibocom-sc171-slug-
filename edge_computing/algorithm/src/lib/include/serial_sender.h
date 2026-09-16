#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class SerialSender {
public:
  using ReceivedCallback = std::function<void(const std::string &)>;

  // Register a callback invoked for every newline-terminated line received on
  // the serial port. Must be called before start().
  void setOnReceived(ReceivedCallback cb) { onReceived_ = std::move(cb); }

  void start(const std::string &portPath, int baudRate);
  void stop();
  void send(const std::string &data);

private:
  void workerLoop();
  void readerLoop();
  bool openPort();
  void closePort();
  bool configurePort();

  std::string portPath_;
  int baudRate_ = 9600;
  int fd_ = -1;

  std::atomic<bool> running_{false};
  std::thread thread_;
  std::thread readerThread_;
  ReceivedCallback onReceived_;

  std::mutex mtx_;
  std::condition_variable cv_;
  std::deque<std::string> pending_;

  // Serializes fd_ lifecycle (open/close) and read/write against the reader
  // and writer threads.
  std::mutex portMtx_;
};
