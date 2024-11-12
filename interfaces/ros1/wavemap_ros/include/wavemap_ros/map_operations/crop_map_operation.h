#ifndef WAVEMAP_ROS_MAP_OPERATIONS_CROP_MAP_OPERATION_H_
#define WAVEMAP_ROS_MAP_OPERATIONS_CROP_MAP_OPERATION_H_

#include <memory>
#include <string>
#include <utility>

#include <wavemap/core/config/config_base.h>
#include <wavemap/core/map/map_base.h>
#include <wavemap/core/utils/time/stopwatch.h>
#include <wavemap/pipeline/map_operations/map_operation_base.h>

#include "wavemap_ros/utils/tf_transformer.h"

namespace wavemap {
/**
 * Config struct for map cropping operations.
 */
struct CropMapOperationConfig : public ConfigBase<CropMapOperationConfig, 5> {
  //! Time period controlling how often the map is cropped.
  Seconds<FloatingPoint> once_every = 10.f;

  //! Name of the TF frame to treat as the center point. Usually the robot's
  //! body frame. When the cropper runs, all voxels that are further than
  //! `radius` from this point are deleted.
  std::string body_frame = "body";

  // TODO(victorr): Explain
  Seconds<FloatingPoint> tf_delay = 0.0f;

  //! Distance beyond which to remove nodes from the map.
  Meters<FloatingPoint> radius;

  //! Maximum resolution at which to crop the map. Defaults to the maximum
  //! resolution. Can be reduced to save computation time, at the cost of more
  //! jagged edges.
  Meters<FloatingPoint> max_update_resolution = 0.f;

  static MemberMap memberMap;

  bool isValid(bool verbose) const override;
};

class CropMapOperation : public MapOperationBase {
 public:
  CropMapOperation(const CropMapOperationConfig& config,
                   MapBase::Ptr occupancy_map,
                   std::shared_ptr<TfTransformer> transformer,
                   std::string world_frame);

  bool shouldRun(const ros::Time& current_time);

  void run(bool force_run) override;

 private:
  const CropMapOperationConfig config_;
  const std::shared_ptr<TfTransformer> transformer_;
  const std::string world_frame_;
  ros::Time last_run_timestamp_;
  Stopwatch timer_;

  const FloatingPoint min_cell_width_ = occupancy_map_->getMinCellWidth();
  const IndexElement termination_height_ =
      min_cell_width_ < config_.max_update_resolution
          ? static_cast<IndexElement>(std::round(
                std::log2(config_.max_update_resolution / min_cell_width_)))
          : 0;
};
}  // namespace wavemap

#endif  // WAVEMAP_ROS_MAP_OPERATIONS_CROP_MAP_OPERATION_H_
