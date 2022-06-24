#include "wavemap_2d/integrator/scan_integrator/coarse_to_fine/coarse_to_fine_integrator.h"

#include <stack>

#include "wavemap_2d/data_structure/volumetric/volumetric_quadtree_interface.h"
#include "wavemap_2d/indexing/ndtree_index.h"

namespace wavemap_2d {
void CoarseToFineIntegrator::integratePointcloud(
    const PosedPointcloud<>& pointcloud) {
  if (!isPointcloudValid(pointcloud)) {
    return;
  }

  // TODO(victorr): Check that the pointcloud's angular resolution is lower than
  //                the angular uncertainty of the beam model. This is necessary
  //                since this measurement integrator assumes the beams don't
  //                overlap, i.e. for each sample point we only evaluate the
  //                contribution from the nearest beam.

  // Compute the range image and the scan's AABB
  // TODO(victorr): Make this configurable
  // TODO(victorr): Avoid reallocating the range image (zero and reuse instead)
  const auto range_image = std::make_shared<RangeImage>(
      computeRangeImage(pointcloud, -kHalfPi, kHalfPi, pointcloud.size()));
  RangeImageIntersector range_image_intersector(range_image);

  // Get a pointer to the underlying specialized quadtree data structure
  auto occupancy_map =
      dynamic_cast<VolumetricQuadtreeInterface*>(occupancy_map_.get());
  if (!occupancy_map) {
    LOG(FATAL) << "Coarse to fine integrator can only be used with "
                  "quadtree-based volumetric data structures.";
    return;
  }

  // Recursively update all relevant cells
  const Transformation T_CW = pointcloud.getPose().inverse();
  const FloatingPoint min_cell_width = occupancy_map->getMinCellWidth();

  std::stack<QuadtreeIndex> stack;
  for (const QuadtreeIndex& node_index :
       occupancy_map->getFirstChildIndices()) {
    stack.emplace(node_index);
  }
  while (!stack.empty()) {
    const auto current_node = std::move(stack.top());
    stack.pop();

    const AABB<Point> W_cell_aabb =
        convert::nodeIndexToAABB(current_node, min_cell_width);
    const RangeImageIntersector::IntersectionType intersection_type =
        range_image_intersector.determineIntersectionType(pointcloud.getPose(),
                                                          W_cell_aabb);
    if (intersection_type ==
        RangeImageIntersector::IntersectionType::kFullyUnknown) {
      continue;
    }

    const FloatingPoint node_width = W_cell_aabb.width<0>();
    const Point W_node_center =
        W_cell_aabb.min + Vector::Constant(node_width / 2.f);
    const Point C_node_center = T_CW * W_node_center;
    FloatingPoint d_C_cell = C_node_center.norm();
    constexpr FloatingPoint kUnitCubeHalfDiagonal = 1.41421356237f / 2.f;
    const FloatingPoint bounding_sphere_radius =
        kUnitCubeHalfDiagonal * node_width;
    if (current_node.height == 0 ||
        isApproximationErrorAcceptable(intersection_type, d_C_cell,
                                       bounding_sphere_radius)) {
      FloatingPoint angle_C_cell = RangeImage::bearingToAngle(C_node_center);
      FloatingPoint sample =
          computeUpdateForCell(*range_image, d_C_cell, angle_C_cell);
      occupancy_map->addToCellValue(current_node, sample);
      continue;
    }

    for (QuadtreeIndex::RelativeChild relative_child_idx = 0;
         relative_child_idx < QuadtreeIndex::kNumChildren;
         ++relative_child_idx) {
      stack.emplace(current_node.computeChildIndex(relative_child_idx));
    }
  }
}

RangeImage CoarseToFineIntegrator::computeRangeImage(
    const PosedPointcloud<>& pointcloud, FloatingPoint min_angle,
    FloatingPoint max_angle, Eigen::Index num_beams) {
  RangeImage range_image(min_angle, max_angle, num_beams);

  for (const auto& C_point : pointcloud.getPointsLocal()) {
    // Filter out noisy points and compute point's range
    if (C_point.hasNaN()) {
      LOG(WARNING) << "Skipping measurement whose endpoint contains NaNs:\n"
                   << C_point;
      continue;
    }
    const FloatingPoint range = C_point.norm();
    if (1e3 < range) {
      LOG(INFO) << "Skipping measurement with suspicious length: " << range;
      continue;
    }

    // Add the point to the range image
    const RangeImageIndex range_image_index =
        range_image.bearingToNearestIndex(C_point);
    range_image[range_image_index] = range;
  }

  return range_image;
}
}  // namespace wavemap_2d
