#include "serial_sender.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

void SerialSender::start(const std::string &portPath, int baudRate) {
  portPath_ = portPath;
  baudRate_ = baudRate;
  running_ = true;
  {
    std::lock_guard<std::mutex> lock(portMtx_);
    openPort();
  }
  thread_ = std::thread(&SerialSender::workerLoop, this);
  readerThread_ = std::thread(&SerialSender::readerLoop, this);
}

void SerialSender::stop() {
  running_ = false;
  cv_.notify_one();
  if (thread_.joinable())
    thread_.join();
  if (readerThread_.joinable())
    readerThread_.join();
  {
    std::lock_guard<std::mutex> lock(portMtx_);
    closePort();
  }
}

void SerialSender::send(const std::string &data) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    pending_.push_back(data);
  }
  cv_.notify_one();
  std::cout << "[Serial] Sent: " << data << std::endl;
}

static speed_t baudToSpeed(int baud) {
  switch (baud) {
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  case 230400:
    return B230400;
  case 460800:
    return B460800;
  case 921600:
    return B921600;
  default:
    return B9600;
  }
}

bool SerialSender::openPort() {
  if (fd_ >= 0)
    return true;

  fd_ = ::open(portPath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    std::cerr << "[Serial] Failed to open " << portPath_ << ": "
              << std::strerror(errno) << std::endl;
    return false;
  }

  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags >= 0)
    fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);

  if (!configurePort()) {
    closePort();
    return false;
  }

  tcflush(fd_, TCIOFLUSH);

  int status;
  if (ioctl(fd_, TIOCMGET, &status) == 0) {
    status &= ~TIOCM_DTR;
    status &= ~TIOCM_RTS;
    ioctl(fd_, TIOCMSET, &status);
  } else {
    std::cerr << "[Serial] Warning: Failed to set DTR/RTS: "
              << std::strerror(errno) << std::endl;
  }

  std::cout << "[Serial] Opened " << portPath_ << " at " << baudRate_ << " baud"
            << std::endl;
  return true;
}

void SerialSender::closePort() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SerialSender::configurePort() {
  struct termios tty;
  if (tcgetattr(fd_, &tty) != 0) {
    std::cerr << "[Serial] tcgetattr failed: " << std::strerror(errno)
              << std::endl;
    return false;
  }

  cfsetospeed(&tty, baudToSpeed(baudRate_));
  cfsetispeed(&tty, baudToSpeed(baudRate_));

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag |= CREAD | CLOCAL;

  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | ISTRIP);
  tty.c_oflag &= ~OPOST;

  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    std::cerr << "[Serial] tcsetattr failed: " << std::strerror(errno)
              << std::endl;
    return false;
  }
  return true;
}

void SerialSender::workerLoop() {
  while (running_) {
    std::string data;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait_for(lock, std::chrono::seconds(1),
                   [this] { return !pending_.empty() || !running_; });
      if (!running_ && pending_.empty())
        break;
      if (pending_.empty())
        continue;
      data = std::move(pending_.front());
      pending_.pop_front();
    }

    bool writeFailed = false;
    ssize_t totalWritten = 0;
    {
      std::lock_guard<std::mutex> plock(portMtx_);
      if (fd_ < 0 && !openPort()) {
        writeFailed = true;
      } else {
        ssize_t remaining = static_cast<ssize_t>(data.size());
        while (remaining > 0) {
          ssize_t written =
              ::write(fd_, data.c_str() + totalWritten, remaining);
          if (written < 0) {
            std::cerr << "[Serial] Write failed: " << std::strerror(errno)
                      << std::endl;
            writeFailed = true;
            break;
          }
          totalWritten += written;
          remaining -= written;
        }

        if (!writeFailed) {
          tcdrain(fd_);
          std::cout << "[Serial] Sent " << totalWritten << " bytes: ";
          for (ssize_t i = 0; i < totalWritten; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(c) << ' ';
          }
          std::cout << std::dec << "(" << data << ")" << std::endl;
        } else {
          closePort();
        }
      }
    }

    if (writeFailed) {
      std::lock_guard<std::mutex> lock(mtx_);
      pending_.push_front(std::move(data));
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void SerialSender::readerLoop() {
  std::string line;
  while (running_) {
    int fd = -1;
    {
      std::lock_guard<std::mutex> plock(portMtx_);
      if (fd_ < 0)
        openPort();
      fd = fd_;
    }

    if (fd < 0) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    char buf[256];
    ssize_t n;
    {
      std::lock_guard<std::mutex> plock(portMtx_);
      if (fd_ < 0) {
        line.clear();
        continue;
      }
      n = ::read(fd_, buf, sizeof(buf));
    }

    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      std::cerr << "[Serial] Read error: " << std::strerror(errno)
                << std::endl;
      std::lock_guard<std::mutex> plock(portMtx_);
      closePort();
      line.clear();
      continue;
    }
    if (n == 0) {
      continue;
    }

    for (ssize_t i = 0; i < n; ++i) {
      char c = buf[i];
      if (c == '\n' || c == '\r') {
        if (!line.empty()) {
          std::cout << "[Serial] Recv: " << line << std::endl;
          if (onReceived_)
            onReceived_(line);
          line.clear();
        }
      } else {
        line += c;
      }
    }
  }
}
