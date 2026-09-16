#include "bytetrack.h"

#include "iou_utils.h"

#include <algorithm>
#include <cmath>
#include <tuple>

struct BYTETracker::STrack {
  cv::Rect_<float> tlwh;
  cv::RotatedRect rbox;
  float score;
  int classId;
  int trackId = -1;
  int frameId = 0;
  int trackletLen = 0;
  int startFrame = 0;
  bool isActivated = false;

  cv::KalmanFilter kf;
  cv::Mat measurement;
  bool predicted = false;

  STrack(cv::Rect_<float> rect, cv::RotatedRect rb, float s, int cls)
      : tlwh(rect), rbox(rb), score(s), classId(cls), kf(8, 4, 0),
        measurement(cv::Mat::zeros(4, 1, CV_32F)) {
    initKalmanFilter();
  }

  void initKalmanFilter() {
    cv::Mat &F = kf.transitionMatrix;
    F.setTo(0);
    for (int i = 0; i < 8; ++i)
      F.at<float>(i, i) = 1.0f;
    F.at<float>(0, 4) = 1.0f;
    F.at<float>(1, 5) = 1.0f;
    F.at<float>(2, 6) = 1.0f;
    F.at<float>(3, 7) = 1.0f;

    cv::Mat &H = kf.measurementMatrix;
    H.setTo(0);
    H.at<float>(0, 0) = 1.0f;
    H.at<float>(1, 1) = 1.0f;
    H.at<float>(2, 2) = 1.0f;
    H.at<float>(3, 3) = 1.0f;

    cv::setIdentity(kf.processNoiseCov, cv::Scalar(1e-2));
    kf.processNoiseCov.at<float>(4, 4) = 5e-2;
    kf.processNoiseCov.at<float>(5, 5) = 5e-2;
    kf.processNoiseCov.at<float>(6, 6) = 5e-2;
    kf.processNoiseCov.at<float>(7, 7) = 5e-2;

    cv::setIdentity(kf.measurementNoiseCov, cv::Scalar(1e-1));

    cv::setIdentity(kf.errorCovPost, cv::Scalar(10.0));

    float cx = tlwh.x + tlwh.width / 2.0f;
    float cy = tlwh.y + tlwh.height / 2.0f;
    kf.statePost.at<float>(0) = cx;
    kf.statePost.at<float>(1) = cy;
    kf.statePost.at<float>(2) = tlwh.width;
    kf.statePost.at<float>(3) = tlwh.height;
  }

  void predict() {
    if (kf.statePost.at<float>(2) + kf.statePost.at<float>(6) <= 0)
      kf.statePost.at<float>(6) = 0;
    if (kf.statePost.at<float>(3) + kf.statePost.at<float>(7) <= 0)
      kf.statePost.at<float>(7) = 0;

    kf.predict();

    float cx = kf.statePre.at<float>(0);
    float cy = kf.statePre.at<float>(1);
    float w = std::max(kf.statePre.at<float>(2), 1.0f);
    float h = std::max(kf.statePre.at<float>(3), 1.0f);
    tlwh.x = cx - w / 2.0f;
    tlwh.y = cy - h / 2.0f;
    tlwh.width = w;
    tlwh.height = h;

    predicted = true;
  }

  void activateTrack(int tid, int fid) {
    trackId = tid;
    frameId = fid;
    startFrame = fid;
    trackletLen = 0;
    isActivated = true;

    float cx = tlwh.x + tlwh.width / 2.0f;
    float cy = tlwh.y + tlwh.height / 2.0f;
    kf.statePost.at<float>(0) = cx;
    kf.statePost.at<float>(1) = cy;
    kf.statePost.at<float>(2) = tlwh.width;
    kf.statePost.at<float>(3) = tlwh.height;
    kf.statePost.at<float>(4) = 0;
    kf.statePost.at<float>(5) = 0;
    kf.statePost.at<float>(6) = 0;
    kf.statePost.at<float>(7) = 0;
    cv::setIdentity(kf.errorCovPost, cv::Scalar(10.0));
  }

  void reActivate(const std::shared_ptr<STrack> &newTrack, int fid) {
    float cx = newTrack->tlwh.x + newTrack->tlwh.width / 2.0f;
    float cy = newTrack->tlwh.y + newTrack->tlwh.height / 2.0f;
    kf.statePost.at<float>(0) = cx;
    kf.statePost.at<float>(1) = cy;
    kf.statePost.at<float>(2) = newTrack->tlwh.width;
    kf.statePost.at<float>(3) = newTrack->tlwh.height;
    kf.statePost.at<float>(4) = 0;
    kf.statePost.at<float>(5) = 0;
    kf.statePost.at<float>(6) = 0;
    kf.statePost.at<float>(7) = 0;
    cv::setIdentity(kf.errorCovPost, cv::Scalar(10.0));

    tlwh = newTrack->tlwh;
    rbox = newTrack->rbox;
    score = newTrack->score;
    classId = newTrack->classId;
    frameId = fid;
    trackletLen = 0;
    isActivated = true;
    predicted = false;
  }

  void updateTrackState(const std::shared_ptr<STrack> &newTrack, int fid) {
    frameId = fid;
    trackletLen++;

    if (!predicted)
      kf.predict();

    float cx = newTrack->tlwh.x + newTrack->tlwh.width / 2.0f;
    float cy = newTrack->tlwh.y + newTrack->tlwh.height / 2.0f;
    measurement.at<float>(0) = cx;
    measurement.at<float>(1) = cy;
    measurement.at<float>(2) = newTrack->tlwh.width;
    measurement.at<float>(3) = newTrack->tlwh.height;
    kf.correct(measurement);

    float ccx = kf.statePost.at<float>(0);
    float ccy = kf.statePost.at<float>(1);
    float w = std::max(kf.statePost.at<float>(2), 1.0f);
    float h = std::max(kf.statePost.at<float>(3), 1.0f);
    tlwh.x = ccx - w / 2.0f;
    tlwh.y = ccy - h / 2.0f;
    tlwh.width = w;
    tlwh.height = h;

    rbox = newTrack->rbox;
    score = newTrack->score;
    classId = newTrack->classId;
    predicted = false;
  }

  cv::Rect_<float> getTLBR() const {
    return cv::Rect_<float>(tlwh.x, tlwh.y, tlwh.width, tlwh.height);
  }
};

BYTETracker::BYTETracker(int frameRate, int trackBuffer, float trackThresh,
                         float matchThresh)
    : trackThresh_(trackThresh), matchThresh_(matchThresh) {
  maxTimeLost_ = trackBuffer;
  if (frameRate > 0)
    maxTimeLost_ = static_cast<int>(static_cast<float>(trackBuffer) /
                                    30.0f * static_cast<float>(frameRate));
}

BYTETracker::~BYTETracker() = default;

std::vector<TrackObject>
BYTETracker::update(const std::vector<Detection> &detections) {
  frameId_++;

  std::vector<std::shared_ptr<STrack>> detStracks;
  detStracks.reserve(detections.size());
  for (const auto &det : detections) {
    cv::Rect br = det.rbox.boundingRect();
    cv::Rect_<float> rect(static_cast<float>(br.x), static_cast<float>(br.y),
                          static_cast<float>(br.width),
                          static_cast<float>(br.height));
    detStracks.push_back(
        std::make_shared<STrack>(rect, det.rbox, det.confidence, det.classId));
  }

  std::vector<std::shared_ptr<STrack>> highDets, lowDets;
  for (auto &s : detStracks) {
    if (s->score >= trackThresh_)
      highDets.push_back(s);
    else if (s->score >= 0.1f)
      lowDets.push_back(s);
  }

  for (auto &s : trackedStracks_)
    s->predict();

  auto pool = trackedStracks_;
  pool.insert(pool.end(), lostStracks_.begin(), lostStracks_.end());
  int numTracked = static_cast<int>(trackedStracks_.size());

  auto distMat = iouDistance(pool, highDets);
  std::vector<std::pair<int, int>> matches1;
  std::vector<int> uPool1, uDet1;
  if (pool.empty()) {
    for (int j = 0; j < static_cast<int>(highDets.size()); ++j)
      uDet1.push_back(j);
  } else {
    linearAssignment(distMat, matchThresh_, matches1, uPool1, uDet1);
  }

  std::vector<bool> poolMatched(pool.size(), false);
  for (auto &[pi, di] : matches1) {
    pool[pi]->updateTrackState(highDets[di], frameId_);
    poolMatched[pi] = true;
  }

  std::vector<std::shared_ptr<STrack>> uTracks;
  uTracks.reserve(uPool1.size());
  for (int idx : uPool1)
    uTracks.push_back(pool[idx]);

  auto distMat2 = iouDistance(uTracks, lowDets);
  std::vector<std::pair<int, int>> matches2;
  std::vector<int> uTracks2, uDet2;
  if (uTracks.empty()) {
    for (int j = 0; j < static_cast<int>(lowDets.size()); ++j)
      uDet2.push_back(j);
  } else {
    linearAssignment(distMat2, 0.5f, matches2, uTracks2, uDet2);
  }

  for (auto &[ti, di] : matches2) {
    uTracks[ti]->updateTrackState(lowDets[di], frameId_);
    poolMatched[uPool1[ti]] = true;
  }

  std::vector<std::shared_ptr<STrack>> newTracked, newLost;

  for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
    bool wasTracked = (i < numTracked);
    if (poolMatched[i]) {
      newTracked.push_back(pool[i]);
    } else {
      if (wasTracked) {
        pool[i]->isActivated = false;
        newLost.push_back(pool[i]);
      } else {
        if (frameId_ - pool[i]->frameId <= maxTimeLost_)
          newLost.push_back(pool[i]);
      }
    }
  }

  for (int idx : uDet1) {
    highDets[idx]->activateTrack(++trackIdCounter_, frameId_);
    newTracked.push_back(highDets[idx]);
  }

  trackedStracks_ = std::move(newTracked);
  lostStracks_ = std::move(newLost);

  std::vector<TrackObject> results;
  results.reserve(trackedStracks_.size());
  for (auto &s : trackedStracks_) {
    TrackObject obj;
    obj.rbox = s->rbox;
    obj.confidence = s->score;
    obj.classId = s->classId;
    obj.trackId = s->trackId;
    results.push_back(obj);
  }
  return results;
}

float BYTETracker::iou(const cv::Rect_<float> &a, const cv::Rect_<float> &b) {
  return ::computeIoU(a, b);
}

std::vector<std::vector<float>> BYTETracker::iouDistance(
    const std::vector<std::shared_ptr<STrack>> &aTracks,
    const std::vector<std::shared_ptr<STrack>> &bTracks) {
  int nA = static_cast<int>(aTracks.size());
  int nB = static_cast<int>(bTracks.size());
  std::vector<std::vector<float>> dist(nA, std::vector<float>(nB, 1.0f));

  for (int i = 0; i < nA; ++i) {
    cv::Rect_<float> ra = aTracks[i]->getTLBR();
    for (int j = 0; j < nB; ++j) {
      cv::Rect_<float> rb = bTracks[j]->getTLBR();
      dist[i][j] = 1.0f - iou(ra, rb);
    }
  }
  return dist;
}

void BYTETracker::linearAssignment(
    const std::vector<std::vector<float>> &cost, float thresh,
    std::vector<std::pair<int, int>> &matches, std::vector<int> &unmatchedA,
    std::vector<int> &unmatchedB) {
  int nA = static_cast<int>(cost.size());
  int nB = (nA > 0) ? static_cast<int>(cost[0].size()) : 0;

  if (nA == 0) {
    for (int j = 0; j < nB; ++j)
      unmatchedB.push_back(j);
    return;
  }
  if (nB == 0) {
    for (int i = 0; i < nA; ++i)
      unmatchedA.push_back(i);
    return;
  }

  std::vector<std::tuple<float, int, int>> pairs;
  pairs.reserve(nA * nB);
  for (int i = 0; i < nA; ++i)
    for (int j = 0; j < nB; ++j)
      if (cost[i][j] < thresh)
        pairs.emplace_back(cost[i][j], i, j);

  std::sort(pairs.begin(), pairs.end());

  std::vector<bool> matchedA(nA, false);
  std::vector<bool> matchedB(nB, false);

  for (auto &[c, i, j] : pairs) {
    if (!matchedA[i] && !matchedB[j]) {
      matches.emplace_back(i, j);
      matchedA[i] = true;
      matchedB[j] = true;
    }
  }

  for (int i = 0; i < nA; ++i)
    if (!matchedA[i])
      unmatchedA.push_back(i);
  for (int j = 0; j < nB; ++j)
    if (!matchedB[j])
      unmatchedB.push_back(j);
}
