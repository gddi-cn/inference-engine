#include "cross_counter_node_v2.h"
#include "postprocess/cross_border.h"
#include "spdlog/spdlog.h"
#include "types.hpp"
#include <memory>
#include <vector>

namespace gddi {
namespace nodes {

void CrossCounter_v2::on_setup() {}

void CrossCounter_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone || frame->frame_info->ext_info.back().map_class_label.empty()) {
        output_result_(frame);
        return;
    }

    if (!cross_border_) {
        int width = frame->frame_info->width();
        int height = frame->frame_info->height();

        std::map<std::string, std::array<int, 2>> label_count;
        for (const auto &[_, value] : frame->frame_info->ext_info.back().map_class_label) {
            label_count[value] = {0, 0};
        }

        for (const auto &[key, points] : regions_with_label_) {
            auto points_size = points.size();
            spdlog::debug("[");
            if (points_size == 2) {
                std::vector<Point2i> line_points;
                line_points.emplace_back(Point2i{int(points[0][0] * width), int(points[0][1] * height)});
                line_points.emplace_back(Point2i{int(points[1][0] * width), int(points[1][1] * height)});
                border_points_.emplace_back(line_points);
                cross_count_.emplace_back(label_count);
            } else {
                for (size_t i = 0; i < points_size; i++) {
                    std::vector<Point2i> line_points;
                    line_points.emplace_back(
                        Point2i{int(points[i % points_size][0] * width), int(points[i % points_size][1] * height)});
                    line_points.emplace_back(Point2i{int(points[(i + 1) % points_size][0] * width),
                                                     int(points[(i + 1) % points_size][1] * height)});
                    border_points_.emplace_back(line_points);
                    cross_count_.emplace_back(label_count);
                }
            }
            spdlog::debug("]");
        }

        cross_border_ = std::make_unique<CrossBorder>();
        cross_border_->init_border(border_points_, int(margin_ * width));
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);

    std::map<int, Rect2f> rects;
    auto &back_ext_info = clone_frame->frame_info->ext_info.back();
    for (const auto &[track_id, item] : back_ext_info.tracked_box) {
        auto box_info = back_ext_info.map_target_box.at(item.target_id);
        rects[track_id] = item.box;
    }

    auto map_target_box = back_ext_info.map_target_box;

    auto result = cross_border_->update_position(rects);
    clone_frame->check_report_callback_ = [result](const std::vector<FrameExtInfo> &ext_info) {
        for (const auto &item : result) {
            if (item.size() > 0) { return FrameType::kReport; }
        }
        return FrameType::kBase;
    };

    int len = result.size();
    for (int i = 0; i < len; i++) {
        for (const auto &[track_id, value] : result[i]) {
            // value: 0 左 1 右
            int target_id = back_ext_info.tracked_box.at(track_id).target_id;
            auto &target_box = map_target_box.at(target_id);
            ++cross_count_[i][frame->frame_info->ext_info.back().map_class_label[target_box.class_id]][value];
        }
    }

    back_ext_info.border_points = border_points_;
    back_ext_info.cross_count = cross_count_;

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
