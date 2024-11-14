#ifndef WAVEMAP_CORE_UTILS_TIME_STOPWATCH_H_
#define WAVEMAP_CORE_UTILS_TIME_STOPWATCH_H_

#include "wavemap/core/utils/time/time.h"

namespace wavemap {
class Stopwatch {
 public:
  void start();
  void stop();

  double getLastEpisodeDuration() const {
    return time::to_seconds<double>(last_episode_duration_);
  }
  double getTotalDuration() const {
    return time::to_seconds<double>(total_duration_);
  }

 private:
  bool running_ = false;

  Timestamp episode_start_time_{};
  Duration last_episode_duration_{};
  Duration total_duration_{};
};
}  // namespace wavemap

#endif  // WAVEMAP_CORE_UTILS_TIME_STOPWATCH_H_
