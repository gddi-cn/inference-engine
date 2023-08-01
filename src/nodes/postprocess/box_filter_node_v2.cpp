#include "box_filter_node_v2.h"
#include "modules/cvrelate/geometry.h"
#include "types.hpp"
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <vector>

namespace gddi {
namespace nodes {

void BoxFilter_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->task_type != TaskType::kCamera || frame->frame_type == FrameType::kNone) {
        // 非摄像头输入 || 空帧 || 目标数为零
        output_result_(frame);
        return;
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    auto &ext_info = clone_frame->frame_info->ext_info.back();

    // BOX
    for (auto iter = ext_info.map_target_box.begin(); iter != ext_info.map_target_box.end();) {
        auto area = iter->second.box.width * iter->second.box.height;
        if (iter->second.box.width < min_width_ || iter->second.box.width > max_width_
            || iter->second.box.height < min_height_ || iter->second.box.height > max_height_) {
            iter = ext_info.map_target_box.erase(iter);
        } else if (area < min_area_ || area > max_area_) {
            iter = ext_info.map_target_box.erase(iter);
        } else if (iter->second.prob < box_prob_) {
            iter = ext_info.map_target_box.erase(iter);
        } else if (vec_box_labels_.count(ext_info.map_class_label.at(iter->second.class_id)) == 0) {
            iter = ext_info.map_target_box.erase(iter);
        } else {
            iter++;
        }
    }

    auto target_box_size = ext_info.map_target_box.size();
    if (target_box_size < min_count_ || target_box_size > max_count_) { return; }

    // OCR
    for (auto iter = ext_info.map_ocr_info.begin(); iter != ext_info.map_ocr_info.end();) {
        std::vector<Point2i> points;
        for (const auto &point : iter->second.points) { points.emplace_back(point.x, point.y); }
        auto area = geometry::polygon_area(points);
        if (area < min_area_ || area > max_area_) {
            iter = ext_info.map_ocr_info.erase(iter);
        } else {
            iter++;
        }
    }

    clone_frame->check_report_callback_ =
        [callback = clone_frame->check_report_callback_](const std::vector<FrameExtInfo> &ext_info) {
            auto status = callback(ext_info);

            // 若过滤后目标数为零, 无需上报
            if (ext_info.back().map_target_box.empty() && ext_info.back().map_ocr_info.empty()) {
                if (status < FrameType::kDisappear) { status = FrameType::kBase; }
            }

            // 若分割结果为空，无需上报
            if (ext_info.back().algo_type == AlgoType::kSegmentation) {
                status = ext_info.back().seg_map.empty() ? FrameType::kBase : FrameType::kReport;
            }

            return status;
        };

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
