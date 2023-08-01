#include "logic_gate_node_v2.h"
#include "node_struct_def.h"
#include <cstdio>
#include <set>

namespace gddi {
namespace nodes {

void LogicGate_v2::on_setup() {
    if (operation_ == "XOR") {
        if (labels_.size() != 2) { quit_runner_(TaskErrorCode::kLogicGate); }
    }

    last_time_point_ = std::chrono::steady_clock::now();
}

void LogicGate_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    auto back_ext_info = frame->frame_info->ext_info.back();
    if (operation_ == "NOT") {
        for (const auto &[idx, target] : back_ext_info.map_target_box) {
            // 标签存在更新时间
            if (labels_.count(back_ext_info.map_class_label.at(target.class_id)) > 0) {
                last_time_point_ = std::chrono::steady_clock::now();
            }
        }
    } else if (operation_ == "AND") {
        std::set<std::string> cur_frame_labels;
        for (const auto &[idx, target] : back_ext_info.map_target_box) {
            if (labels_.count(back_ext_info.map_class_label.at(target.class_id)) > 0) {
                cur_frame_labels.insert(back_ext_info.map_class_label.at(target.class_id));
            }
        }
        // 包含所有标签更新时间
        if (cur_frame_labels.size() != labels_.size()) { last_time_point_ = std::chrono::steady_clock::now(); }
    } else if (operation_ == "OR") {
        for (const auto &[idx, target] : back_ext_info.map_target_box) {
            if (labels_.count(back_ext_info.map_class_label.at(target.class_id)) == 0) {
                last_time_point_ = std::chrono::steady_clock::now();
            }
        }
    } else if (operation_ == "XOR") {
        std::set<std::string> cur_frame_labels;
        for (const auto &[idx, target] : back_ext_info.map_target_box) {
            if (labels_.count(back_ext_info.map_class_label.at(target.class_id)) > 0) {
                cur_frame_labels.insert(back_ext_info.map_class_label.at(target.class_id));
            }
        }
        // 包含其中一个标签更新时间
        if (cur_frame_labels.size() == 1) { last_time_point_ = std::chrono::steady_clock::now(); }
    } else {
        // 默认实时更新
        last_time_point_ = std::chrono::steady_clock::now();
    }

    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_time_point_).count()
        > duration_) {
        last_time_point_ = std::chrono::steady_clock::now();
        frame->check_report_callback_ = [callback =
                                             frame->check_report_callback_](const std::vector<FrameExtInfo> &ext_info) {
            return std::max(FrameType::kReport, callback(ext_info));
        };
        output_result_(frame);
    }
}

}// namespace nodes
}// namespace gddi
