#include "roi_filter_node_v2.h"
#include "modules/cvrelate/geometry.h"

namespace gddi {
namespace nodes {

void RoiFilter_v2::on_setup() {
    if (!regions_.empty()) {
        char buffer[512] = {0};
        spdlog::debug("############################## ROI "
                      "##############################");
        int region_id_ = 0;
        if (regions_with_label_.empty()) {
            for (auto &region : regions_) {
                spdlog::debug("[");
                for (auto &point : region) { spdlog::debug("    [{}, {}]", point[0], point[1]); }
                spdlog::debug("]");
                regions_with_label_[std::to_string(region_id_++)] = region;
            }
        }
        for (auto &[key, points] : regions_with_label_) {
            spdlog::debug("[");
            for (auto &point : points) { spdlog::debug("    [{}, {}]", point[0], point[1]); }
            spdlog::debug("]");
        }
        spdlog::debug("#################################################################");
    }
}

void RoiFilter_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_image_(frame);
        return;
    }

    // 预处理 ROI 区域
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

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    auto &last_ext_info = clone_frame->frame_info->ext_info.back();
    auto tmp_target_box = last_ext_info.map_target_box;
    clone_frame->frame_info->roi_points = roi_points_;

    if (!clone_frame->frame_info->roi_points.empty()) { last_ext_info.map_target_box.clear(); }

    if (!clone_frame->frame_info->ext_info.empty()) {

        for (const auto &[key, points] : roi_points_) {

            // 目标框与ROI区域重叠面积需大于阈值
            for (auto &item : tmp_target_box) {
                std::vector<Point2i> box_points;
                box_points.push_back({int(item.second.box.x), int(item.second.box.y)});
                box_points.push_back({int(item.second.box.x), int(item.second.box.y + item.second.box.height)});
                box_points.push_back(
                    {int(item.second.box.x + item.second.box.width), int(item.second.box.y + item.second.box.height)});
                box_points.push_back({int(item.second.box.x + item.second.box.width), int(item.second.box.y)});

                if (geometry::area_cover_rate(box_points, points) >= threshold_) {
                    item.second.roi_id = key;
                    last_ext_info.map_target_box.insert(item);
                }
            }

            // 姿态关键点需全部落在ROI区域内
            for (auto iter = last_ext_info.map_key_points.begin(); iter != last_ext_info.map_key_points.end();) {
                int index = 0;
                for (const auto &key_point : iter->second) {
                    if (!geometry::point_within_area(Point2i(key_point.x, key_point.y), points)) {
                        last_ext_info.map_target_box.erase(iter->first);
                        iter = last_ext_info.map_key_points.erase(iter);
                        break;
                    }
                    index++;
                }
                if (index == 17) { ++iter; }
            }

            // OCR多边形与ROI区域重叠面积
            for (auto iter = last_ext_info.map_ocr_info.begin(); iter != last_ext_info.map_ocr_info.end();) {
                std::vector<Point2i> ocr_points;
                for (const auto &point : iter->second.points) { ocr_points.push_back({int(point.x), int(point.y)}); }
                if (geometry::area_cover_rate(ocr_points, points) >= threshold_) {
                    ++iter;
                } else {
                    iter = last_ext_info.map_ocr_info.erase(iter);
                }
            }
        }
    }

    output_image_(clone_frame);
}

}// namespace nodes
}// namespace gddi