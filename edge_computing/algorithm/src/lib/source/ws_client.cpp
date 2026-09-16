#include "ws_client.h"

#include "base64.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#include <cstring>
#include <random>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>

struct WsClient::State {
  SSL *ssl = nullptr;
  SSL_CTX *ctx = nullptr;
  int sockfd = -1;
  bool useSSL = false;
  bool connected = false;
  std::string recvBuf;
  LogCallback logCb;
};

WsClient::WsClient() : s_(std::make_unique<State>()) {}

WsClient::~WsClient() {
  disconnect();
}

void WsClient::setLogCallback(LogCallback cb) { s_->logCb = cb; }

struct UrlParts {
  std::string scheme, host, path;
  int port;
  bool useSSL;
};

static bool parseUrl(const std::string &url, UrlParts &out) {
  size_t pos = url.find("://");
  if (pos == std::string::npos)
    return false;
  out.scheme = url.substr(0, pos);
  std::string rest = url.substr(pos + 3);

  out.port = (out.scheme == "wss") ? 443 : 80;
  out.useSSL = (out.scheme == "wss");

  size_t slash = rest.find('/');
  if (slash != std::string::npos) {
    out.host = rest.substr(0, slash);
    out.path = rest.substr(slash);
  } else {
    out.host = rest;
    out.path = "/";
  }

  size_t colon = out.host.rfind(':');
  if (colon != std::string::npos) {
    out.port = std::stoi(out.host.substr(colon + 1));
    out.host = out.host.substr(0, colon);
  }
  return true;
}

static int tcpConnect(const std::string &host, int port) {
  struct addrinfo hints = {}, *res;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) !=
      0)
    return -1;

  int fd = -1;
  for (auto *p = res; p; p = p->ai_next) {
    fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0)
      continue;
    if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0)
      break;
    ::close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

static bool sslWriteAll(SSL *ssl, const std::string &data) {
  size_t sent = 0;
  while (sent < data.size()) {
    int n = SSL_write(ssl, data.data() + sent, data.size() - sent);
    if (n <= 0)
      return false;
    sent += n;
  }
  return true;
}

static bool rawWriteAll(int fd, const std::string &data) {
  size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
    if (n <= 0)
      return false;
    sent += n;
  }
  return true;
}

static int readOnce(bool useSSL, SSL *ssl, int fd, char *buf, size_t len) {
  if (useSSL)
    return SSL_read(ssl, buf, len);
  return ::recv(fd, buf, len, 0);
}

bool WsClient::connect(const std::string &url) {
  UrlParts parts;
  if (!parseUrl(url, parts)) {
    if (s_->logCb)
      s_->logCb("Invalid URL: " + url);
    return false;
  }

  s_->sockfd = tcpConnect(parts.host, parts.port);
  if (s_->sockfd < 0) {
    if (s_->logCb)
      s_->logCb("TCP connect failed: " + parts.host + ":" +
                std::to_string(parts.port));
    return false;
  }

  if (parts.useSSL) {
    s_->ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_default_verify_paths(s_->ctx);
    s_->ssl = SSL_new(s_->ctx);
    SSL_set_fd(s_->ssl, s_->sockfd);
    SSL_set_tlsext_host_name(s_->ssl, parts.host.c_str());
    if (SSL_connect(s_->ssl) <= 0) {
      if (s_->logCb)
        s_->logCb("SSL handshake failed");
      disconnect();
      return false;
    }
    s_->useSSL = true;
  }

  uint8_t keyBytes[16];
  std::random_device rd;
  for (auto &b : keyBytes)
    b = rd();
  std::string wsKey = base64Encode(keyBytes, 16);

  std::string req = "GET " + parts.path + " HTTP/1.1\r\n"
                                            "Host: " +
                    parts.host + "\r\n"
                                 "Upgrade: websocket\r\n"
                                 "Connection: Upgrade\r\n"
                                 "Sec-WebSocket-Key: " +
                    wsKey +
                    "\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n";

  if (s_->useSSL) {
    if (!sslWriteAll(s_->ssl, req)) {
      disconnect();
      return false;
    }
  } else {
    if (!rawWriteAll(s_->sockfd, req)) {
      disconnect();
      return false;
    }
  }

  std::string response;
  while (response.find("\r\n\r\n") == std::string::npos) {
    char buf[4096];
    int n = readOnce(s_->useSSL, s_->ssl, s_->sockfd, buf, sizeof(buf));
    if (n <= 0) {
      disconnect();
      return false;
    }
    response.append(buf, n);
  }

  if (response.find("101") == std::string::npos) {
    if (s_->logCb)
      s_->logCb("WebSocket handshake failed: " + response.substr(0, 64));
    disconnect();
    return false;
  }

  size_t bodyStart = response.find("\r\n\r\n") + 4;
  if (bodyStart < response.size()) {
    s_->recvBuf = response.substr(bodyStart);
  }

  s_->connected = true;
  if (s_->logCb)
    s_->logCb("Connected to " + url);
  return true;
}

void WsClient::disconnect() {
  if (s_->ssl) {
    SSL_shutdown(s_->ssl);
    SSL_free(s_->ssl);
    s_->ssl = nullptr;
  }
  if (s_->ctx) {
    SSL_CTX_free(s_->ctx);
    s_->ctx = nullptr;
  }
  if (s_->sockfd >= 0) {
    ::close(s_->sockfd);
    s_->sockfd = -1;
  }
  s_->connected = false;
  s_->recvBuf.clear();
}

bool WsClient::isConnected() const { return s_->connected; }

bool WsClient::sendText(const std::string &message) {
  std::vector<uint8_t> frame;
  frame.push_back(0x81);

  uint8_t maskKey[4];
  std::random_device rd;
  for (auto &b : maskKey)
    b = rd();

  if (message.size() < 126) {
    frame.push_back(0x80 | (uint8_t)message.size());
  } else if (message.size() < 65536) {
    frame.push_back(0x80 | 126);
    frame.push_back((message.size() >> 8) & 0xFF);
    frame.push_back(message.size() & 0xFF);
  } else {
    frame.push_back(0x80 | 127);
    uint64_t len = message.size();
    for (int i = 7; i >= 0; i--)
      frame.push_back((len >> (i * 8)) & 0xFF);
  }

  frame.insert(frame.end(), maskKey, maskKey + 4);

  for (size_t i = 0; i < message.size(); i++)
    frame.push_back(message[i] ^ maskKey[i % 4]);

  if (s_->useSSL) {
    return sslWriteAll(s_->ssl, std::string(frame.begin(), frame.end()));
  }
  return rawWriteAll(s_->sockfd, std::string(frame.begin(), frame.end()));
}

std::string WsClient::recvText(int timeoutMs) {
  while (true) {
    if (s_->recvBuf.size() >= 2) {
      uint8_t b1 = (uint8_t)s_->recvBuf[1];
      bool masked = b1 & 0x80;
      uint64_t payloadLen = b1 & 0x7F;

      size_t headerSize = 2;
      if (payloadLen == 126)
        headerSize += 2;
      else if (payloadLen == 127)
        headerSize += 8;
      if (masked)
        headerSize += 4;

      if (payloadLen == 126 && s_->recvBuf.size() >= 4) {
        payloadLen = ((uint64_t)(uint8_t)s_->recvBuf[2] << 8) |
                     (uint8_t)s_->recvBuf[3];
      } else if (payloadLen == 127 && s_->recvBuf.size() >= 10) {
        payloadLen = 0;
        for (int i = 0; i < 8; i++)
          payloadLen = (payloadLen << 8) | (uint8_t)s_->recvBuf[2 + i];
      }

      size_t totalSize = headerSize + payloadLen;
      if (s_->recvBuf.size() >= totalSize) {
        uint8_t opcode = (uint8_t)s_->recvBuf[0] & 0x0F;
        std::string payload(s_->recvBuf.begin() + headerSize,
                            s_->recvBuf.begin() + headerSize + payloadLen);

        if (masked) {
          size_t maskOff = headerSize - 4;
          uint8_t m[4];
          for (int i = 0; i < 4; i++)
            m[i] = (uint8_t)s_->recvBuf[maskOff + i];
          for (size_t i = 0; i < payloadLen; i++)
            payload[i] ^= m[i % 4];
        }

        s_->recvBuf.erase(s_->recvBuf.begin(),
                          s_->recvBuf.begin() + totalSize);

        if (opcode == 0x1)
          return payload;
        if (opcode == 0x8) {
          s_->connected = false;
          return "";
        }
        if (opcode == 0x9) {
          std::vector<uint8_t> pong;
          pong.push_back(0x8A);
          pong.push_back((uint8_t)payload.size());
          pong.insert(pong.end(), payload.begin(), payload.end());
          if (s_->useSSL)
            SSL_write(s_->ssl, pong.data(), pong.size());
          else
            ::send(s_->sockfd, pong.data(), pong.size(), 0);
          continue;
        }
        continue;
      }
    }

    if (timeoutMs >= 0 && s_->sockfd >= 0) {
      struct pollfd pfd;
      pfd.fd = s_->sockfd;
      pfd.events = POLLIN;
      pfd.revents = 0;
      int pr = ::poll(&pfd, 1, timeoutMs);
      if (pr == 0)
        return "";
      if (pr < 0) {
        s_->connected = false;
        return "";
      }
      if (!(pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
        s_->connected = false;
        return "";
      }
    }

    char buf[4096];
    int n = readOnce(s_->useSSL, s_->ssl, s_->sockfd, buf, sizeof(buf));
    if (n <= 0) {
      s_->connected = false;
      return "";
    }
    s_->recvBuf.append(buf, n);
  }
}
