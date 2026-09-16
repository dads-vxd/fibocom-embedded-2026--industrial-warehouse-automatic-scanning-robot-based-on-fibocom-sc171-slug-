#include "config.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

AppConfig loadAppConfig(const std::string &configPath) {
  AppConfig cfg;

  try {
    std::ifstream ifs(configPath);
    if (!ifs.is_open())
      return cfg;

    auto j = json::parse(ifs);

    if (j.contains("model_path"))
      cfg.modelPath = j["model_path"].get<std::string>();
    if (j.contains("ws_url"))
      cfg.wsUrl = j["ws_url"].get<std::string>();
    if (j.contains("control_ws_url"))
      cfg.controlWsUrl = j["control_ws_url"].get<std::string>();
    if (j.contains("video_upload_url"))
      cfg.videoUploadUrl = j["video_upload_url"].get<std::string>();
    if (j.contains("blur_threshold"))
      cfg.blurThreshold = j["blur_threshold"].get<double>();
    if (j.contains("serial_enabled"))
      cfg.serialEnabled = j["serial_enabled"].get<bool>();
    if (j.contains("serial_port"))
      cfg.serialPort = j["serial_port"].get<std::string>();
    if (j.contains("serial_baud_rate"))
      cfg.serialBaudRate = j["serial_baud_rate"].get<int>();

    if (j.contains("video_record")) {
      auto &v = j["video_record"];
      if (v.contains("enabled"))
        cfg.videoRecord.enabled = v["enabled"].get<bool>();
      if (v.contains("fps"))
        cfg.videoRecord.fps = v["fps"].get<double>();
      if (v.contains("segment_seconds"))
        cfg.videoRecord.segmentSeconds = v["segment_seconds"].get<int>();
      if (v.contains("max_width"))
        cfg.videoRecord.maxWidth = v["max_width"].get<int>();
      if (v.contains("max_height"))
        cfg.videoRecord.maxHeight = v["max_height"].get<int>();
    }

    if (j.contains("preprocess")) {
      auto &p = j["preprocess"];
      if (p.contains("scale"))
        cfg.preprocess.scale = p["scale"].get<double>();
      if (p.contains("enable_clahe"))
        cfg.preprocess.enable_clahe = p["enable_clahe"].get<bool>();
      if (p.contains("clahe_clip_limit"))
        cfg.preprocess.clahe_clip_limit = p["clahe_clip_limit"].get<double>();
      if (p.contains("enable_sharpen"))
        cfg.preprocess.enable_sharpen = p["enable_sharpen"].get<bool>();
      if (p.contains("sharpen_alpha"))
        cfg.preprocess.sharpen_alpha = p["sharpen_alpha"].get<double>();
      if (p.contains("enable_binary"))
        cfg.preprocess.enable_binary = p["enable_binary"].get<bool>();
      if (p.contains("binary_block_size"))
        cfg.preprocess.binary_block_size = p["binary_block_size"].get<int>();
      if (p.contains("binary_c"))
        cfg.preprocess.binary_c = p["binary_c"].get<int>();
    }

    if (j.contains("cameras")) {
      for (auto &cj : j["cameras"]) {
        CameraConfig cc;
        cc.deviceId = cj.value("device_id", "/dev/video0");
        cc.enabled = cj.value("enabled", true);
        cc.preprocess = cfg.preprocess;
        if (cj.contains("preprocess") && !cj["preprocess"].empty()) {
          auto &p = cj["preprocess"];
          auto loadVal = [&](const char *key, auto &dest) {
            if (p.contains(key))
              dest = p[key].get<std::decay_t<decltype(dest)>>();
          };
          loadVal("scale", cc.preprocess.scale);
          loadVal("enable_clahe", cc.preprocess.enable_clahe);
          loadVal("clahe_clip_limit", cc.preprocess.clahe_clip_limit);
          loadVal("enable_sharpen", cc.preprocess.enable_sharpen);
          loadVal("sharpen_alpha", cc.preprocess.sharpen_alpha);
          loadVal("enable_binary", cc.preprocess.enable_binary);
          loadVal("binary_block_size", cc.preprocess.binary_block_size);
          loadVal("binary_c", cc.preprocess.binary_c);
        }
        cfg.cameras.push_back(cc);
      }
    } else {
      CameraConfig cc;
      if (j.contains("device_id"))
        cc.deviceId = j["device_id"].get<std::string>();
      else
        cc.deviceId = "/dev/video0";
      cc.preprocess = cfg.preprocess;
      cfg.cameras.push_back(cc);
    }

  } catch (const std::exception &e) {
    std::cerr << "Warning: parse config failed: " << e.what() << std::endl;
  }

  return cfg;
}
