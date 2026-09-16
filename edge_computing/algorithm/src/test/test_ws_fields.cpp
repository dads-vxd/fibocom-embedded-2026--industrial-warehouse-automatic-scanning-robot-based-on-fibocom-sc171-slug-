#include "ws_client.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char *argv[]) {
  std::string wsUrl =
      (argc > 1) ? argv[1] : "wss://gt.yelob.vip/ws/device";
  std::string outputPath = (argc > 2) ? argv[2] : "barcode_fields.json";

  WsClient ws;
  ws.setLogCallback(
      [](const std::string &msg) { std::cout << "[WS] " << msg << std::endl; });

  std::cout << "Connecting to " << wsUrl << "..." << std::endl;
  if (!ws.connect(wsUrl)) {
    std::cerr << "WebSocket connection failed" << std::endl;
    return 1;
  }
  std::cout << "Connected" << std::endl;

  json req;
  req["type"] = "get_categories";
  ws.sendText(req.dump());

  std::string respStr = ws.recvText();
  ws.disconnect();

  if (respStr.empty()) {
    std::cerr << "No response from server" << std::endl;
    return 1;
  }

  auto resp = json::parse(respStr);

  if (resp.contains("error")) {
    std::cerr << "Server error: " << resp["error"] << std::endl;
    return 1;
  }

  if (!resp.contains("data") || !resp["data"].is_array()) {
    std::cerr << "No data array in response" << std::endl;
    std::cout << "Raw response:\n" << resp.dump(2) << std::endl;
    return 1;
  }

  auto &categories = resp["data"];
  std::cout << "Got " << categories.size() << " categories" << std::endl;

  for (const auto &cat : categories) {
    std::cout << "  [" << cat["id"] << "] " << cat["name"];
    if (cat.contains("field_defs") && cat["field_defs"].is_array()) {
      std::cout << " (" << cat["field_defs"].size() << " fields)";
    }
    std::cout << std::endl;
  }

  json output;
  output["categories"] = categories;

  std::ofstream ofs(outputPath);
  if (!ofs) {
    std::cerr << "Cannot write to " << outputPath << std::endl;
    return 1;
  }
  ofs << output.dump(2) << std::endl;
  ofs.close();

  std::cout << "\nSaved " << categories.size() << " categories to "
            << outputPath << std::endl;

  return 0;
}
