#include "image_preprocess.h"

PreprocessedImage preprocessImage(const cv::Mat &crop,
                                  const PreprocessConfig &cfg) {
  PreprocessedImage out;

  cv::Mat src = crop;
  if (src.empty())
    return out;

  if (cfg.scale > 1.0) {
    cv::resize(src, src, cv::Size(), cfg.scale, cfg.scale, cv::INTER_LINEAR);
  }

  if (src.channels() == 1)
    out.gray = src.clone();
  else
    cv::cvtColor(src, out.gray, cv::COLOR_BGR2GRAY);

  if (cfg.enable_clahe) {
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(cfg.clahe_clip_limit,
                                                cv::Size(8, 8));
    clahe->apply(out.gray, out.gray);
  }

  if (cfg.enable_sharpen) {
    cv::Mat blurred;
    cv::GaussianBlur(out.gray, blurred, cv::Size(0, 0), 1.5);
    double beta = 1.0 - cfg.sharpen_alpha;
    cv::addWeighted(out.gray, cfg.sharpen_alpha, blurred, beta, 0,
                    out.sharpened_gray);
  }

  if (cfg.enable_binary) {
    const cv::Mat &binSrc =
        cfg.enable_sharpen ? out.sharpened_gray : out.gray;
    int bs = cfg.binary_block_size | 1;
    cv::adaptiveThreshold(binSrc, out.binary, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY,
                          bs, cfg.binary_c);
  }

  if (src.channels() == 3) {
    if (cfg.enable_clahe) {
      cv::Mat lab;
      cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);
      std::vector<cv::Mat> ch;
      cv::split(lab, ch);
      cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(cfg.clahe_clip_limit,
                                                  cv::Size(8, 8));
      clahe->apply(ch[0], ch[0]);
      cv::merge(ch, lab);
      cv::cvtColor(lab, out.bgr, cv::COLOR_Lab2BGR);
    } else {
      out.bgr = src.clone();
    }

    if (cfg.enable_sharpen) {
      cv::Mat blurred;
      cv::GaussianBlur(out.bgr, blurred, cv::Size(0, 0), 1.5);
      double beta = 1.0 - cfg.sharpen_alpha;
      cv::addWeighted(out.bgr, cfg.sharpen_alpha, blurred, beta, 0, out.bgr);
    }
  } else {
    cv::cvtColor(out.gray, out.bgr, cv::COLOR_GRAY2BGR);
  }

  return out;
}
