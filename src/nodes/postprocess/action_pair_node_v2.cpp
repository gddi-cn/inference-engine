#include "action_pair_node_v2.h"
#include "node_msg_def.h"
#include "node_struct_def.h"
#include "utils.hpp"
#include <opencv2/core/hal/interface.h>
#include <spdlog/spdlog.h>

namespace gddi {
namespace nodes {

void ActionPair_v2::on_setup() {
    auto action_param = nlohmann::json::parse(action_param_);

    std::map<int, gddi::PosePoint> pose_points;
    for (const auto &[key, item] : action_param.items()) {
        gddi::PosePoint pose_point;
        pose_point.weight = item["weight"].get<double>();
        auto orientation = item["orientation"].get<std::string>();
        if (orientation == "vertical") {
            pose_point.orientation = gddi::Orientation::kVertical;
        } else if (orientation == "horizontal") {
            pose_point.orientation = gddi::Orientation::kHorizontal;
        } else if (orientation == "score") {
            pose_point.orientation = gddi::Orientation::kScore;
        }
        pose_points.insert(std::make_pair(atoi(key.c_str()), std::move(pose_point)));
    }

    std::string standard_buffer;
    utils::read_file("config/" + standard_file_, standard_buffer);
    auto standard_json_obj = nlohmann::json::parse(standard_buffer);

    int index = 0;
    for (const auto &item : standard_json_obj) {
        for (const auto &key_points : item) {
            index = 0;
            std::vector<gddi::nodes::PoseKeyPoint> vec_key_point;
            for (const auto &key_point : key_points) {
                auto values = key_point.get<std::vector<float>>();
                gddi::nodes::PoseKeyPoint pose_key_point;
                pose_key_point.number = index++;
                pose_key_point.x = values[0];
                pose_key_point.y = values[1];
                pose_key_point.prob = values[2];
                vec_key_point.emplace_back(std::move(pose_key_point));
            }
            standard_range_.emplace_back(std::move(vec_key_point));
        }
    }

    action_pair_ = std::make_unique<gddi::ActionPair>(pose_points);
}

void ActionPair_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    auto clone_frame = std::make_shared<msgs::cv_frame>(frame);

    auto &back_ext_info = clone_frame->frame_info->ext_info.back();
    for (const auto &[track_id, action] : back_ext_info.cur_action_score) {
        // 分数小于阈值
        if (action.second >= threshold_) { action_score_[track_id][action.first].emplace_back(action.second); }

        // 每个关键点的分数
        // std::map<std::pair<int, int>, std::vector<double>> pairs;
        // auto distance = action_pair_->pair_action(prev_ext_info.action_key_points.at(tid), standard_range_, pairs);

        // printf("class: %d, id: %d, 两个动作的距离是 %f, 该得分越低越好\n", cid, tid, distance);
        // for (const auto &[key, values] : pairs) {
        //     printf("\t第%d次动作, 第%d帧和第%d帧匹配\n", action_count_[idx], key.first, key.second);

        //     int value_index = 0;
        //     for (const auto &value : values) {
        //         printf("\t\t关键点%d的得分是%f\n", value_index++, value);
        //     }
        // }
    }

    for (auto iter = action_score_.begin(); iter != action_score_.end();) {
        if (back_ext_info.tracked_box.count(iter->first) > 0) {
            back_ext_info.tracked_box.at(iter->first).prob = action_score_.at(iter->first).begin()->second.back();
            back_ext_info.sum_action_scores[iter->first] = action_score_.at(iter->first);
            ++iter;
        } else {
            iter = action_score_.erase(iter);
        }
    }

    output_result_(clone_frame);
}

}// namespace nodes
}// namespace gddi
