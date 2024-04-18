#ifndef WAVEMAP_ROS_WAVEMAP_SERVER_H_
#define WAVEMAP_ROS_WAVEMAP_SERVER_H_

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <glog/logging.h>
#include <ros/ros.h>
#include <wavemap/common.h>
#include <wavemap/config/config_base.h>
#include <wavemap/indexing/index_hashes.h>
#include <wavemap/integrator/integrator_base.h>
#include <wavemap/map/map_base.h>
#include <wavemap/utils/thread_pool.h>
#include <wavemap/utils/time/stopwatch.h>
#include <wavemap/utils/time/time.h>
#include <wavemap_ros/logging_level.h>

#include "wavemap_ros/input_handler/input_handler.h"
#include "wavemap_ros/tf_transformer.h"

namespace wavemap {
/**
 * Config struct for wavemap's ROS server.
 */
struct WavemapServerConfig : ConfigBase<WavemapServerConfig, 8, LoggingLevel> {
  //! Name of the coordinate frame in which to store the map.
  //! Will be used as the frame_id for ROS TF lookups.
  std::string world_frame = "odom";
  //! Time period controlling how often the map is thresholded.
  //! To disable thresholding, set it to a negative number [not recommended].
  Seconds<FloatingPoint> thresholding_period = 1.f;
  //! Time period controlling how often the map is pruned.
  //! To disable pruning, set it to a negative number.
  Seconds<FloatingPoint> pruning_period = 10.f;
  //! Time period controlling how often the map is published.
  //! To disable map publishing, set it to a negative number.
  Seconds<FloatingPoint> publication_period = 10.f;
  //! Maximum number of blocks to transmit per wavemap map message.
  //! Used to control the maximum message size. Only works in combination with
  //! hash-based map data structures.
  int max_num_blocks_per_msg = 1000;
  //! Maximum number of threads to use.
  //! Defaults to the number of threads supported by the CPU.
  int num_threads =
      std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
  //! Minimum severity level for ROS logging messages to be logged.
  LoggingLevel logging_level = LoggingLevel::kInfo;
  //! Whether or not to allow resetting the map through the reset_map service.
  bool allow_reset_map_service = false;

  static MemberMap memberMap;

  bool isValid(bool verbose) const override;
};

class WavemapServer {
 public:
  WavemapServer(ros::NodeHandle nh, ros::NodeHandle nh_private);
  WavemapServer(ros::NodeHandle nh, ros::NodeHandle nh_private,
                const WavemapServerConfig& config);

  void publishMap(bool republish_whole_map = false);
  bool saveMap(const std::filesystem::path& file_path) const;
  bool loadMap(const std::filesystem::path& file_path);

  InputHandler* addInput(const param::Value& integrator_params,
                         const ros::NodeHandle& nh, ros::NodeHandle nh_private);

  MapBase::Ptr getMap() { return occupancy_map_; }
  MapBase::ConstPtr getMap() const { return occupancy_map_; }

  void setMapUpdatedCallback(std::function<void(const MapBase&)> callback);

 private:
  const WavemapServerConfig config_;

  MapBase::Ptr occupancy_map_;

  std::shared_ptr<TfTransformer> transformer_;
  std::shared_ptr<ThreadPool> thread_pool_;
  std::vector<std::unique_ptr<InputHandler>> input_handlers_;

  void subscribeToTimers(const ros::NodeHandle& nh);
  ros::Timer map_pruning_timer_;
  ros::Timer map_thresholding_timer_;
  ros::Timer map_publication_timer_;

  void subscribeToTopics(ros::NodeHandle& nh);

  void advertiseTopics(ros::NodeHandle& nh_private);
  ros::Publisher map_pub_;

  void advertiseServices(ros::NodeHandle& nh_private);
  ros::ServiceServer republish_whole_map_srv_;
  ros::ServiceServer reset_map_srv_;
  ros::ServiceServer save_map_srv_;
  ros::ServiceServer load_map_srv_;

  // Map block publishing
  // NOTE: For hashed map types, such as HashedWaveletOctree and
  //       HashedChunkedWaveletOctree, we support incremental map transmissions
  //       which only include the blocks that changed since the last
  //       transmission, unless republish_whole_map is set to true.
  //       In case the number of blocks that changed exceeds
  //       config_.max_num_blocks_per_msg, the map update is transferred using
  //       multiple messages. This can be useful when transmitting the maps over
  //       unreliable networks, where smaller packets can perform better in
  //       terms of packet loss, or when the change is so large that
  //       transmitting it as a single message would exceed the maximum ROS
  //       message size (1GB).
  template <typename HashedMapT>
  void publishHashedMap(HashedMapT* hashed_map,
                        bool republish_whole_map = false);
  Timestamp last_map_pub_time_;
};
}  // namespace wavemap

#include "wavemap_ros/impl/wavemap_server_inl.h"

#endif  // WAVEMAP_ROS_WAVEMAP_SERVER_H_
