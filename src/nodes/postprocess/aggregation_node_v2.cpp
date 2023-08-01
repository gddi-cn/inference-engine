#include "aggregation_node_v2.h"
#include "modules/cvrelate/geometry.h"

namespace gddi {
namespace nodes {

void Aggregation_v2::on_setup() { input_endpoint_count_ = get_input_endpoint_count(); }

void Aggregation_v2::on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_image_(frame);
        return;
    }

    if (cache_frame_info_[frame->frame_info->video_frame_idx].size() < input_endpoint_count_ - 1) {
        cache_frame_info_[frame->frame_info->video_frame_idx].emplace_back(frame->frame_info);
        return;
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);

    auto &back_ext_info = clone_frame->frame_info->ext_info.back();
    size_t target_index = back_ext_info.map_target_box.size();
    for (auto &frame_info : cache_frame_info_[clone_frame->frame_info->video_frame_idx]) {
        size_t label_index = back_ext_info.map_class_label.size();
        for (auto &key_points : frame_info->ext_info.back().map_key_points) {
            back_ext_info.map_key_points[target_index] = key_points.second;
        }
        for (auto &[target_id, box_info] : frame_info->ext_info.back().map_target_box) {
            box_info.class_id += label_index;
            back_ext_info.map_target_box[target_index] = box_info;

            if (frame_info->ext_info.back().map_key_points.count(target_id) > 0) {
                back_ext_info.map_key_points[target_index] = frame_info->ext_info.back().map_key_points.at(target_id);
            }
        }
        for (const auto &[key, value] : frame_info->ext_info.back().map_class_label) {
            back_ext_info.map_class_label[key + label_index] = value;
        }
        for (const auto &[key, value] : frame_info->ext_info.back().map_class_color) {
            back_ext_info.map_class_color[key + label_index] = value;
        }

        for (const auto &region : frame_info->roi_points) {
            auto iter = clone_frame->frame_info->roi_points.begin();
            for (; iter != clone_frame->frame_info->roi_points.end(); ++iter) {
                if (geometry::has_intersection(iter->second, region.second)) { break; }
            }
            if (iter != clone_frame->frame_info->roi_points.end()) {
                iter->second = geometry::merge_region(iter->second, region.second);
            } else {
                clone_frame->frame_info->roi_points.insert(region);
            }
        }

        ++target_index;
    }

    cache_frame_info_.erase(clone_frame->frame_info->video_frame_idx);

    // // 打印 map_target_box 成员信息
    // for (const auto &item : back_ext_info.map_target_box) {
    //     spdlog::info("map_target_box: {}, {}", item.first, item.second.class_id);
    // }

    // // 打印 map_key_points 成员信息
    // for (const auto &item : back_ext_info.map_key_points) {
    //     spdlog::info("map_key_points: {}, {}", item.first, item.second.size());
    // }

    // for (const auto &[key, value] : back_ext_info.map_class_label) {
    //     spdlog::info("map_class_label: {}, {}", key, value);
    // }

    // if (back_ext_info.map_target_box.size() > 0) {
    //     spdlog::info("============================================================");
    // }

    output_image_(clone_frame);
}

}// namespace nodes
}// namespace gddi