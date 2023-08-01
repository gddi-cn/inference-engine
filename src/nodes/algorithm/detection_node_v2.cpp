#include "detection_node_v2.h"
#include <cstdio>
#include <mutex>
#include <utility>

namespace gddi {
namespace nodes {

void Detection_v2::on_setup() {
    detecter_ = algo::make_algo_impl();

    detecter_->register_init_callback([this](const std::vector<std::string> &vec_labels) {
        try {
            int index = 0;
            for (auto &item : vec_labels) {
                if (mod_label_objs_.count(item) > 0 && mod_label_objs_[item]["checked"].get<bool>()) {
                    map_class_label_[index] = mod_label_objs_[item]["label"].get<std::string>();
                    map_class_color_[index] = mod_label_objs_[item]["color"].get<std::vector<int>>();
                }
                index++;
            }
        } catch (const std::exception &e) {
            spdlog::error(e.what());
            quit_runner_(TaskErrorCode::kInference);
        }
    });

    detecter_->register_infer_callback(
        [this](const int64_t frame_idx, const AlgoType type, const std::vector<algo::AlgoOutput> &vec_output) {
            std::shared_ptr<msgs::cv_frame> frame;
            {
                std::lock_guard<std::mutex> lck(cache_mtx_);
                frame = cache_frames_.at(frame_idx);
                cache_frames_.erase(frame_idx);
            }
            if (frame->frame_info->infer_frame_idx == frame_idx) {
                frame->frame_info->ext_info.emplace_back(parser_detection_output(vec_output));
                output_result_(frame);
            } else {
                spdlog::error("expect frame: {}, but dequeue frame: {}", frame_idx, frame->frame_info->infer_frame_idx);
            }
        });

    future_ = std::async(std::launch::async, [this]() { return detecter_->init(parms_); });
}

void Detection_v2::on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    // auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
    // clone_frame->frame_info->ext_info.emplace_back(
    //     nodes::FrameExtInfo(AlgoType::kDetection, "1", "11", 0.01));
    // cache_frames_.insert(std::make_pair(clone_frame->frame_info->frame_idx, clone_frame));
    // auto dst_frame = cache_frames_.at(clone_frame->frame_info->frame_idx);
    // cache_frames_.erase(clone_frame->frame_info->frame_idx);
    // output_result_(dst_frame);
    // return;

    // 模型加载处理
    if (future_.valid()) {
        if (frame->task_type == TaskType::kImage || frame->task_type == TaskType::kAsyncImage) {
            future_.get();
        } else if (future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (!future_.get()) {
                // 模型加载失败
                quit_runner_(TaskErrorCode::kInference);
                return;
            }
        } else {
            // 模型加载未完成
            std::vector<algo::AlgoOutput> vec_output;
            frame->frame_info->ext_info.emplace_back(parser_detection_output(vec_output));
            output_result_(frame);
            return;
        }
    }

    try {
        if (frame->infer_frame_rate < parms_.frame_rate
            || fmod(frame->frame_info->infer_frame_idx, frame->infer_frame_rate * 1.0 / parms_.frame_rate) < 1) {
            auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
            clone_frame->infer_frame_rate = parms_.frame_rate;

            if (clone_frame->frame_info->ext_info.empty()) {
                {
                    std::lock_guard<std::mutex> lck(cache_mtx_);
                    if (cache_frames_.size() < 10) {
                        cache_frames_.insert(std::make_pair(clone_frame->frame_info->infer_frame_idx, clone_frame));
                    } else {
                        spdlog::warn("task: {}, drop: {}", clone_frame->task_name,
                                     clone_frame->frame_info->infer_frame_idx);
                        return;
                    }
                }

                if (clone_frame->task_type == TaskType::kImage) {
                    // 同步
                    detecter_->inference(clone_frame->task_name, clone_frame->frame_info, algo::InferType::kSync);
                } else {
                    // 一阶段异步;
                    detecter_->inference(clone_frame->task_name, clone_frame->frame_info, algo::InferType::kAsync);
                }
            } else {
                // 多阶段同步
                std::map<int, std::vector<algo::AlgoOutput>> outputs;
                auto algo_type = detecter_->inference(clone_frame->task_name, clone_frame->frame_info, outputs);
                clone_frame->frame_info->ext_info.emplace_back(parser_detection_output(outputs));
                output_result_(clone_frame);
            }
        }

        // if (frame->frame_info->frame_idx % 4 == 0) {
        //     std::lock_guard<std::mutex> ulk(mtx_);
        //     for (auto &item : map_idx_frame_) {
        //         item.second->frame_info->ext_info.emplace_back(parser_detection_output({}));
        //         output_result_(item.second);
        //     }
        //     map_idx_frame_.clear();
        // }
    } catch (const std::exception &e) {
        spdlog::error(e.what());
        quit_runner_(TaskErrorCode::kInference);
    }
}

nodes::FrameExtInfo Detection_v2::parser_detection_output(const std::vector<algo::AlgoOutput> &vec_output) {
    nodes::FrameExtInfo ext_info(AlgoType::kDetection, parms_.mod_id, parms_.mod_name, parms_.mod_thres);
    ext_info.map_class_label = map_class_label_;
    for (auto &item : map_class_color_) {
        ext_info.map_class_color[item.first] = {(float)item.second[0], (float)item.second[1], (float)item.second[2], 1};
    }

    int index = 1;
    for (const auto &output : vec_output) {
        if (map_class_label_.count(output.class_id) > 0 && output.prob >= parms_.mod_thres) {
            ext_info.map_target_box[index] =
                nodes::BoxInfo{.prev_id = index, .class_id = output.class_id, .prob = output.prob, .box = output.box};
            ++index;
        }
    }

    return ext_info;
}

nodes::FrameExtInfo Detection_v2::parser_detection_output(const std::map<int, std::vector<algo::AlgoOutput>> &outputs) {
    nodes::FrameExtInfo ext_info(AlgoType::kDetection, parms_.mod_id, parms_.mod_name, parms_.mod_thres);
    ext_info.map_class_label = map_class_label_;
    for (auto &item : map_class_color_) {
        ext_info.map_class_color[item.first] = {(float)item.second[0], (float)item.second[1], (float)item.second[2], 1};
    }

    int index = 0;
    for (const auto &[track_id, output] : outputs) {
        for (const auto &item : output) {
            if (map_class_label_.count(item.class_id) > 0 && item.prob >= parms_.mod_thres) {
                ext_info.map_target_box.insert(std::make_pair(
                    index,
                    BoxInfo{.prev_id = track_id, .class_id = item.class_id, .prob = item.prob, .box = item.box}));
                ++index;
            }
        }
    }

    return ext_info;
}

}// namespace nodes
}// namespace gddi