#include "roi_perspective_transform_v2.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <map>
#include <modules/cvrelate/geometry.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace gddi {
namespace nodes {

const int transform_width_ = 1788;
const int transform_height_ = 3569;

const std::map<std::string, std::vector<Point2i>> roi_rect{
    {"1", {{0, 0}, {150, 0}, {150, 150}, {0, 150}}},
    {"2", {{transform_width_ - 150, 0}, {transform_width_, 0}, {transform_width_, 150}, {transform_width_ - 150, 150}}},
    {"3",
     {{0, transform_height_ / 2 - 50},
      {100, transform_height_ / 2 - 50},
      {100, transform_height_ / 2 + 50},
      {0, transform_height_ / 2 + 50}}},
    {"4",
     {{transform_width_ - 100, transform_height_ / 2 - 50},
      {transform_width_, transform_height_ / 2 - 50},
      {transform_width_, transform_height_ / 2 + 50},
      {transform_width_ - 100, transform_height_ / 2 + 50}}},
    {"5",
     {{0, transform_height_ - 150}, {150, transform_height_ - 150}, {150, transform_height_}, {0, transform_height_}}},
    {"6",
     {{transform_width_ - 150, transform_height_ - 150},
      {transform_width_, transform_height_ - 150},
      {transform_width_, transform_height_},
      {transform_width_ - 150, transform_height_}}},
    {"12",
     {{transform_width_ / 2 + 80 / 2, 2640 + 80 / 2},
      {transform_width_ / 2 + 80 * 4 / 2, 2640 + 80 / 2 + 80 * 3},
      {transform_width_ / 2 - 80 * 4 / 2, 2640 + 80 / 2 + 80 * 3},
      {transform_width_ / 2 - 80 / 2, 2640 + 80 / 2}}},
    {"13",
     {{610 - 80 / 2, 737 - 80 / 2},
      {610 + 80 / 2, 737 - 80 / 2},
      {610 + 80 / 2, 737 + 80 / 2},
      {610 - 80 / 2, 737 + 80 / 2}}},
    {"14",
     {{1175 - 80 / 2, 737 - 80 / 2},
      {1175 + 80 / 2, 737 - 80 / 2},
      {1175 + 80 / 2, 737 + 80 / 2},
      {1175 - 80 / 2, 737 + 80 / 2}}},
    {"15",
     {{transform_width_ / 2 - 80 / 2, 737 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, 737 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, 737 + 80 / 2},
      {transform_width_ / 2 - 80 / 2, 737 + 80 / 2}}},
    {"16",
     {{transform_width_ / 2 - 80 / 2, transform_height_ / 2 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, transform_height_ / 2 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, transform_height_ / 2 + 80 / 2},
      {transform_width_ / 2 - 80 / 2, transform_height_ / 2 + 80 / 2}}},
    {"17",
     {{transform_width_ / 2 - 80 / 2, 2630 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, 2630 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, 2630 + 80 / 2},
      {transform_width_ / 2 - 80 / 2, 2630 + 80 / 2}}},
    {"18",
     {{transform_width_ / 2 - 80 / 2, 3240 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, 3240 - 80 / 2},
      {transform_width_ / 2 + 80 / 2, 3240 + 80 / 2},
      {transform_width_ / 2 - 80 / 2, 3240 + 80 / 2}}}};

void RoiPerspectiveTransform_v2::on_setup() {
    for (auto &[key, points] : regions_with_label_) {
        spdlog::debug("[");
        for (auto &point : points) { spdlog::debug("    [{}, {}]", point[0], point[1]); }
        spdlog::debug("]");
    }
    spdlog::debug("#################################################################");
}

void RoiPerspectiveTransform_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_image_(frame);
        return;
    }

    if (roi_points_.empty()) {
        for (auto &[key, points] : regions_with_label_) {
            std::vector<Point2i> new_region_points;
            for (auto &point : points) {
                new_region_points.emplace_back(point[0] * frame->frame_info->width(),
                                               point[1] * frame->frame_info->height());
            }

            std::map<std::string, std::vector<Point2i>> tmp_roi_points;
            for (auto iter = roi_points_.begin(); iter != roi_points_.end();) {
                if (geometry::has_intersection(iter->second, new_region_points)) {
                    tmp_roi_points.insert(*iter);
                    roi_points_.erase(iter);
                } else {
                    iter++;
                }
            }

            if (!tmp_roi_points.empty()) {
                roi_points_[key] = geometry::merge_region(new_region_points, tmp_roi_points);
            } else {
                roi_points_[key] = new_region_points;
            }
        }
    }

    if (transform_matrix_.empty()) {
        std::vector<cv::Point2f> input_quad;
        for (auto &point : roi_points_.begin()->second) { input_quad.emplace_back(point.x, point.y); }

        if (input_quad.size() != 4) {
            quit_runner_(TaskErrorCode::kPerspectiveTransform);
            return;
        }

        std::vector<cv::Point2f> output_quad{{0, 0},
                                             {transform_width_, 0},
                                             {transform_width_, transform_height_},
                                             {0, transform_height_}};

        transform_matrix_ = cv::getPerspectiveTransform(input_quad, output_quad);
    }

    for (auto &[target_id, bbox] : frame->frame_info->ext_info.back().map_target_box) {
        std::vector<Point2i> box_points;
        box_points.push_back({int(bbox.box.x), int(bbox.box.y)});
        box_points.push_back({int(bbox.box.x), int(bbox.box.y + bbox.box.height)});
        box_points.push_back({int(bbox.box.x + bbox.box.width), int(bbox.box.y + bbox.box.height)});
        box_points.push_back({int(bbox.box.x + bbox.box.width), int(bbox.box.y)});

        // 判断球出界
        for (const auto &[key, points] : roi_points_) {
            auto area_cover_rate = geometry::area_cover_rate(box_points, points);
            if (area_cover_rate == 0) {
                bbox.roi_id = "20";// 20为无效区域
            }
        }
    }

    frame->frame_info->roi_points = roi_rect;

#ifdef WITH_NVIDIA
    cv::cuda::warpPerspective(*frame->frame_info->src_frame->data, *frame->frame_info->src_frame->data,
                              transform_matrix_, cv::Size(transform_width_, transform_height_));
#elif defined(WITH_MLU220)
    // auto transform_image = image_wrapper::image_to_mat(frame->frame_info->src_frame->data);
    // cv::warpPerspective(transform_image, transform_image, transform_matrix_,
    //                     cv::Size(transform_width_, transform_height_));
    // frame->frame_info->src_frame->data = image_wrapper::mat_to_image(transform_image);
#endif

    for (auto &[_, value] : frame->frame_info->ext_info.back().map_target_box) {
        std::vector<cv::Point2f> bbox{{value.box.x, value.box.y},
                                      {value.box.x + value.box.width, value.box.y},
                                      {value.box.x + value.box.width, value.box.y + value.box.height},
                                      {value.box.x, value.box.y + value.box.height}};
        std::vector<cv::Point2f> transform_bbox;
        cv::perspectiveTransform(bbox, transform_bbox, transform_matrix_);
        value.box.x = std::min({transform_bbox[0].x, transform_bbox[1].x, transform_bbox[2].x, transform_bbox[3].x});
        value.box.y = std::min({transform_bbox[0].y, transform_bbox[1].y, transform_bbox[2].y, transform_bbox[3].y});
        value.box.width = std::max({transform_bbox[0].x, transform_bbox[1].x, transform_bbox[2].x, transform_bbox[3].x})
            - value.box.x;
        value.box.height =
            std::max({transform_bbox[0].y, transform_bbox[1].y, transform_bbox[2].y, transform_bbox[3].y})
            - value.box.y;

        // cv::rectangle(transform_image, cv::Rect{value.box.x, value.box.y, value.box.width, value.box.height},
        //               cv::Scalar(255, 0, 0), 2);

        std::vector<Point2i> box_points;
        box_points.push_back({int(value.box.x), int(value.box.y)});
        box_points.push_back({int(value.box.x), int(value.box.y + value.box.height)});
        box_points.push_back({int(value.box.x + value.box.width), int(value.box.y + value.box.height)});
        box_points.push_back({int(value.box.x + value.box.width), int(value.box.y)});

        // 判断球在哪个区域
        for (const auto &[key, points] : roi_rect) {
            auto area_cover_rate = geometry::area_cover_rate(box_points, points);
            if (area_cover_rate >= 0.5) {
                value.roi_id = key;
                spdlog::debug("roi_id: {}, target_id: {}", value.roi_id, value.track_id);
            }
        }

        if (frame->frame_info->ext_info.back().tracked_box.count(value.track_id) > 0) {
            frame->frame_info->ext_info.back().tracked_box.at(value.track_id).box = value.box;
        }
    }

    // cv::imwrite("perspective_transform_v2.jpg", transform_image);

    output_image_(frame);
}

}// namespace nodes
}// namespace gddi