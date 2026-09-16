#include "ws_client.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

// End-to-end test for the device control channel.
//
// Default (listener) mode: connect to /ws/device-control and LOOP-READ whatever
// the backend forwards -- i.e. {"command":"..."} messages sent from the
// browser's manual-control page. Each command is printed together with the
// serial bytes the real device would write. Reconnects automatically on drop.
//
//   test_control                       # listen on wss://gt.yelob.vip
//   test_control ws://localhost        # listen on local docker stack
//
// --selftest: automated one-shot round-trip (stands up BOTH a device and a
// frontend connection, verifies command + log in both directions, exits 0/1).
//
//   test_control --selftest
//   test_control --selftest wss://gt.yelob.vip
//
// Note: connecting to /ws/device-control on a live server takes over the
// single device slot (the backend replaces any existing device connection),
// so a real device will briefly drop and reconnect.

using json = nlohmann::json;

// "发现障碍物" as UTF-8 bytes (source file is ASCII-safe).
static const std::string OBSTACLE =
    "\xe5\x8f\x91\xe7\x8e\xb0\xe9\x9a\x9c\xe7\xa2\x8d\xe7\x89\xa9";

static std::string SerialFor(const std::string &cmd) {
  if (cmd == "forward")  return "14";
  if (cmd == "backward") return "18";
  if (cmd == "left")     return "13";
  if (cmd == "right")    return "12";
  if (cmd == "pause")    return "14";
  return "";
}

static int Fail(const std::string &msg) {
  std::cerr << "[FAIL] " << msg << std::endl;
  return 1;
}

// ---- default mode: listen on /ws/device-control, loop-read commands ----
static int RunListener(const std::string &deviceUrl) {
  int backoff = 2;
  while (true) {
    WsClient device;
    device.setLogCallback(
        [](const std::string &m) { std::cout << "[ws] " << m << std::endl; });

    std::cout << "Connecting device -> " << deviceUrl << " ..." << std::endl;
    if (!device.connect(deviceUrl)) {
      std::cerr << "[listener] connect failed; retry in " << backoff << "s"
                << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(backoff));
      backoff = std::min(backoff * 2, 30);
      continue;
    }
    backoff = 2;
    std::cout << "[listener] connected. Waiting for commands (Ctrl+C to quit)."
              << std::endl;

    while (device.isConnected()) {
      // Poll with a 1s timeout so the loop can notice disconnects and we can
      // periodically log liveness without blocking forever.
      std::string raw = device.recvText(1000);
      if (!device.isConnected())
        break;
      if (raw.empty())
        continue;

      try {
        auto j = json::parse(raw);
        if (j.contains("command") && j["command"].is_string()) {
          std::string cmd = j["command"].get<std::string>();
          std::string s = SerialFor(cmd);
          if (!s.empty())
            std::cout << "[recv] command='" << cmd << "'  ->  serial write "
                      << s << std::endl;
          else
            std::cout << "[recv] unknown command: " << cmd << std::endl;
        } else if (j.contains("log")) {
          std::cout << "[recv] log: " << j["log"] << std::endl;
        } else {
          std::cout << "[recv] " << raw << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr << "[recv] parse error: " << e.what() << " (" << raw << ")"
                  << std::endl;
      }
    }
    std::cerr << "[listener] disconnected; reconnecting in " << backoff << "s"
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(backoff));
    backoff = std::min(backoff * 2, 30);
  }
  return 0;
}

// ---- --selftest mode: automated round-trip ----
static int RunSelftest(const std::string &base) {
  const std::string deviceUrl = base + "/ws/device-control";
  const std::string frontendUrl = base + "/ws/control";

  WsClient::LogCallback log = [](const std::string &m) {
    std::cout << "[ws] " << m << std::endl;
  };

  std::cout << "Connecting device   -> " << deviceUrl << std::endl;
  WsClient device;
  device.setLogCallback(log);
  if (!device.connect(deviceUrl))
    return Fail("device could not connect to " + deviceUrl);
  std::cout << "device connected" << std::endl;

  std::cout << "Connecting frontend -> " << frontendUrl << std::endl;
  WsClient frontend;
  frontend.setLogCallback(log);
  if (!frontend.connect(frontendUrl)) {
    device.disconnect();
    return Fail("frontend could not connect to " + frontendUrl);
  }
  std::cout << "frontend connected" << std::endl;

  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  json cmd;
  cmd["command"] = "forward";
  std::cout << "\n[1] frontend sends {\"command\":\"forward\"}" << std::endl;
  if (!frontend.sendText(cmd.dump()))
    return Fail("frontend sendText(command) failed");
  std::string gotCmd = device.recvText(5000);
  if (gotCmd.empty())
    return Fail("device did not receive the command within 5s");
  std::cout << "    device received: " << gotCmd << std::endl;
  try {
    if (json::parse(gotCmd).value("command", "") != "forward")
      return Fail("device got unexpected command payload");
  } catch (const std::exception &e) {
    return Fail(std::string("device parse error: ") + e.what());
  }

  json lg;
  lg["log"] = OBSTACLE;
  std::cout << "\n[2] device sends {\"log\":\"" << OBSTACLE << "\"}" << std::endl;
  if (!device.sendText(lg.dump()))
    return Fail("device sendText(log) failed");
  std::string gotLog = frontend.recvText(5000);
  if (gotLog.empty())
    return Fail("frontend did not receive the log within 5s");
  std::cout << "    frontend received: " << gotLog << std::endl;
  try {
    if (json::parse(gotLog).value("log", "") != OBSTACLE)
      return Fail("frontend got unexpected log payload");
  } catch (const std::exception &e) {
    return Fail(std::string("frontend parse error: ") + e.what());
  }

  device.disconnect();
  frontend.disconnect();
  std::cout << "\n[PASS] control channel round-trip OK" << std::endl;
  return 0;
}

int main(int argc, char *argv[]) {
  bool selftest = false;
  std::string base = "wss://gt.yelob.vip";
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--selftest" || a == "-s")
      selftest = true;
    else
      base = a;
  }

  if (selftest)
    return RunSelftest(base);

  return RunListener(base + "/ws/device-control");
}
