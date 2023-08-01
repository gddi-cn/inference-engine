#include "action_counter_node_v2.h"
#include "node_msg_def.h"
#include "postprocess/naive_counter.h"
#include <opencv2/core/hal/interface.h>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

namespace gddi {
namespace nodes {

void ActionCounter_v2::on_setup() {
    auto action_param = nlohmann::json::parse(action_param_);

    std::vector<NaiveConfigItem> naive_config;
    for (const auto &item : action_param) {
        NaiveConfigItem naive_point;
        std::string method = item["method"].get<std::string>();
        if (method == "point:x") {
            naive_point.method = NavieMethod::kPoint_X;
        } else if (method == "point" || method == "point:y") {
            naive_point.method = NavieMethod::kPoint_Y;
        } else if (method == "distance") {
            naive_point.method = NavieMethod::kDistance;
        } else if (method == "angle") {
            naive_point.method = NavieMethod::kAngle;
        } else {
            spdlog::error("Unsupport method: {}", method);
        }
        naive_point.landmarks = item["landmarks"].get<std::vector<int>>();
        if (item.count("threshold") > 0) { naive_point.threshold = item["threshold"].get<float>(); }
        naive_config.emplace_back(std::move(naive_point));
    }

    naive_couter_ = std::make_unique<NaiveCounter>(std::move(naive_config));
    track_smoother_ = std::make_unique<TrackSmoothing>(SmoothingType::kKalman);
}

void ActionCounter_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    auto &last_ext_info = clone_frame->frame_info->ext_info.back();
    last_ext_info.action_type = ActionType::kCount;

    for (const auto &[track_id, item] : last_ext_info.tracked_box) {
        std::vector<std::vector<nodes::PoseKeyPoint>> action_key_points;
        track_smoother_->smooth_keypoints(last_ext_info.map_key_points.at(item.target_id));
        naive_couter_->update_key_point(track_id, frame->frame_info->width(), frame->frame_info->height(),
                                        last_ext_info.map_key_points.at(item.target_id), action_key_points);
        if (!action_key_points.empty()) {
            last_ext_info.action_key_points[track_id] = std::move(action_key_points);
        }
    }

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
