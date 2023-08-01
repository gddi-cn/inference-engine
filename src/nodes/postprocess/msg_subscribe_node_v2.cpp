#include "msg_subscribe_node_v2.h"
#include "base64.h"
#include "modules/network/zmq_socket.h"
#include "node_struct_def.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <spdlog/spdlog.h>

namespace gddi {
namespace nodes {

void MsgSubscribe_v2::on_setup() { last_event_time_ = time(NULL); }

void MsgSubscribe_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    // 模型未加载完成 或者 空帧
    if (frame->frame_type == FrameType::kNone || frame->frame_info->ext_info.back().algo_type == AlgoType::kUndefined) {
        return;
    }

    if (time(NULL) - last_event_time_ < time_interval_) { return; }
    last_event_time_ = time(NULL);

    auto topic = frame->task_name;
    auto pos = frame->task_name.find('_');
    if (pos != frame->task_name.npos) { topic = frame->task_name.substr(0, pos); }
    topic = topic + "_MsgSubscribe_v2";

    // try {
    std::string buffer;
    auto event_id = boost::uuids::to_string(boost::uuids::random_generator()());
    if (frame_info_to_string(frame->task_name, event_id, frame->frame_info, buffer)) {
        network::ZmqSocket::get_instance("tcp://*:9000").send(topic, buffer);
    }
    // } catch (const std::exception &e) { spdlog::error("Failed to send message: {}, error: {}", topic, e.what()); }
}

bool MsgSubscribe_v2::frame_info_to_string(const std::string &task_name, const std::string &event_id,
                                           const std::shared_ptr<nodes::FrameInfo> &frame_info, std::string &buffer) {
    auto event_obj = nlohmann::json::object();
    event_obj["event_id"] = event_id;
    event_obj["task_id"] = task_name;
    event_obj["created"] = frame_info->timestamp;
    event_obj["details"] = nlohmann::json::array();

    const auto &last_ext_info = frame_info->ext_info.back();

    auto model_obj = nlohmann::json::object();
    model_obj["model_id"] = last_ext_info.mod_id;
    model_obj["model_name"] = last_ext_info.mod_name;
    model_obj["model_type"] = last_ext_info.algo_type;
    model_obj["model_thres"] = last_ext_info.mod_thres;
    model_obj["targets"] = nlohmann::json::array();

    auto regions = nlohmann::json::array();
    for (auto &[key, points] : frame_info->roi_points) {
        auto point_array = nlohmann::json::array();
        for (auto &point : points) { point_array.push_back({point.x, point.y}); }
        regions.emplace_back(point_array);
    }
    model_obj["regions"] = regions;

    if (last_ext_info.algo_type == AlgoType::kAction) {
        // 无需上报数据
        if (last_ext_info.map_key_points.empty() && !every_frame_) { return false; }

        auto target_obj = nlohmann::json::object();
        for (const auto &[track_id, actions] : last_ext_info.sum_action_scores) {
            if (last_ext_info.map_key_points.count(track_id) == 0) { continue; }

            target_obj["target_id"] = track_id;
            for (const auto &[action_id, scores] : actions) {
                auto action_obj = nlohmann::json::object();
                for (const auto &score : scores) { action_obj["scores"].emplace_back(int(score * 10000) / 10000.0); }
                action_obj["type"] = last_ext_info.map_class_label.at(action_id);
                target_obj["actions"].emplace_back(std::move(action_obj));
            }

            for (auto &point : last_ext_info.map_key_points.at(track_id)) {
                auto box_obj = nlohmann::json::object();
                box_obj["x"] = (int)point.x;
                box_obj["y"] = (int)point.y;
                box_obj["idx"] = point.number;
                box_obj["prob"] = int(point.prob * 10000) / 10000.0;
                target_obj["key_points"].emplace_back(std::move(box_obj));
            }

            model_obj["targets"].emplace_back(std::move(target_obj));
        }
    } else {
        if (last_ext_info.map_target_box.empty() && !every_frame_) { return false; }

        for (const auto &[idx, target] : last_ext_info.map_target_box) {
            auto target_obj = nlohmann::json::object();
            target_obj["id"] = idx;
            target_obj["prev_id"] = target.prev_id;

            target_obj["label"] = last_ext_info.map_class_label.at(target.class_id);
            target_obj["prob"] = int(target.prob * 10000 + 0.5) / 10000.0;
            target_obj["roi_id"] = target.roi_id;

            target_obj["box"]["left_top_x"] = target.box.x;
            target_obj["box"]["left_top_y"] = target.box.y;
            target_obj["box"]["right_bottom_x"] = target.box.x + target.box.width;
            target_obj["box"]["right_bottom_y"] = target.box.y + target.box.height;
            target_obj["color"] = last_ext_info.map_class_color.at(target.class_id).to_array<int>();

            if (last_ext_info.algo_type == AlgoType::kPose || last_ext_info.algo_type == AlgoType::kFace) {
                target_obj["key_points"] = nlohmann::json::array();
                for (auto &point : last_ext_info.map_key_points.at(idx)) {
                    auto box_obj = nlohmann::json::object();
                    box_obj["x"] = point.x;
                    box_obj["y"] = point.y;
                    box_obj["idx"] = point.number;
                    box_obj["prob"] = point.prob;
                    target_obj["key_points"].push_back(box_obj);
                }
            }

            if (last_ext_info.algo_type == AlgoType::kFace) {
                int buffer_size = 0;
                std::vector<uchar> buffer;
#if defined(WITH_BM1684)
                image_wrapper::image_jpeg_enc(
                    (frame_info->ext_info.rbegin() + 1)->crop_images.second->at(target.prev_id), buffer, buffer_size);
#elif defined(WITH_MLU220) || defined(WITH_MLU270) || defined(WITH_MLU370)
                image_wrapper::image_jpeg_enc((frame_info->ext_info.rbegin() + 1)->crop_images[target.prev_id], buffer, buffer_size);
#else
                cv::imencode(".jpg", (frame_info->ext_info.rbegin() + 1)->crop_images[target.prev_id], buffer);
#endif

                auto feature_obj = nlohmann::json::object();
                feature_obj["vector"] = last_ext_info.features.at(target.track_id);
                feature_obj["image"] = Base64::encode(buffer);
                target_obj["feature_info"] = feature_obj;
            }

            model_obj["targets"].push_back(target_obj);
        }
    }

    event_obj["details"].push_back(model_obj);

    // spdlog::info("{}", event_obj.dump());

    buffer = event_obj.dump();

    return true;
}

}// namespace nodes
}// namespace gddi
