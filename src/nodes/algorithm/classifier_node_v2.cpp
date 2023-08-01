#include "classifier_node_v2.h"
#include <vector>

namespace gddi {
namespace nodes {

void Classifier_v2::on_setup() {
    classifier_ = algo::make_algo_impl();

    classifier_->register_init_callback([this](const std::vector<std::string> &vec_labels) {
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

    classifier_->register_infer_callback([this](const int64_t frame_idx, const AlgoType type,
                                                const std::vector<algo::AlgoOutput> &vec_output) {
        std::shared_ptr<msgs::cv_frame> frame;
        if (que_frame_.try_dequeue(frame)) {
            if (frame->frame_info->infer_frame_idx == frame_idx) {
                frame->frame_info->ext_info.emplace_back(parser_classification_output(vec_output));
                output_result_(frame);
            } else {
                spdlog::error("expect frame: {}, but dequeue frame: {}", frame_idx, frame->frame_info->infer_frame_idx);
            }
        } else {
            spdlog::error("nullptr frame: {}", frame_idx);
        }
    });

    if (!classifier_->init(parms_)) { quit_runner_(TaskErrorCode::kInference); }
}

void Classifier_v2::on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == FrameType::kNone) {
        output_result_(frame);
        return;
    }

    try {
        if (frame->infer_frame_rate < parms_.frame_rate
            || fmod(frame->frame_info->infer_frame_idx, frame->infer_frame_rate * 1.0 / parms_.frame_rate) < 1) {
            auto clone_frame = std::make_shared<msgs::cv_frame>(frame);
            clone_frame->infer_frame_rate = parms_.frame_rate;

            if (frame->frame_info->ext_info.empty()) {
                que_frame_.enqueue(clone_frame);

                if (frame->task_type == TaskType::kImage) {
                    // 同步
                    classifier_->inference(clone_frame->task_name, clone_frame->frame_info, algo::InferType::kSync);
                } else {
                    // 一阶段异步
                    classifier_->inference(clone_frame->task_name, clone_frame->frame_info, algo::InferType::kAsync);
                }
            } else {
                // 多阶段同步
                std::map<int, std::vector<algo::AlgoOutput>> outputs;
                auto algo_type = classifier_->inference(clone_frame->task_name, clone_frame->frame_info, outputs);
                clone_frame->frame_info->ext_info.emplace_back(parser_classification_output(outputs));
                output_result_(clone_frame);
            }
        }
    } catch (const std::exception &e) {
        spdlog::error(e.what());
        quit_runner_(TaskErrorCode::kInference);
    }
}

nodes::FrameExtInfo Classifier_v2::parser_classification_output(const std::vector<algo::AlgoOutput> &vec_output) {
    nodes::FrameExtInfo ext_info(AlgoType::kClassification, parms_.mod_id, parms_.mod_name, parms_.mod_thres);
    ext_info.map_class_label = map_class_label_;
    for (auto &item : map_class_color_) {
        ext_info.map_class_color[item.first] = {(float)item.second[0], (float)item.second[1], (float)item.second[2], 1};
    }

    int index = 1;
    for (const auto &output : vec_output) {
        if (map_class_label_.count(output.class_id) > 0) {
            ext_info.map_target_box[index] = {.prev_id = index, .class_id = output.class_id, .prob = output.prob};
            ++index;
        }
    }

    return ext_info;
}

nodes::FrameExtInfo
Classifier_v2::parser_classification_output(const std::map<int, std::vector<algo::AlgoOutput>> &outputs) {
    nodes::FrameExtInfo ext_info(AlgoType::kClassification, parms_.mod_id, parms_.mod_name, parms_.mod_thres);
    ext_info.map_class_label = map_class_label_;
    for (auto &item : map_class_color_) {
        ext_info.map_class_color[item.first] = {(float)item.second[0], (float)item.second[1], (float)item.second[2], 1};
    }

    for (const auto &[idx, output] : outputs) {
        for (const auto &item : output) {
            if (map_class_label_.count(item.class_id) > 0) {
                ext_info.map_target_box[(int)ext_info.map_target_box.size() + 1] = {.prev_id = idx,
                                                                                    .class_id = item.class_id,
                                                                                    .prob = item.prob};
            }
        }
    }

    return ext_info;
}

}// namespace nodes
}// namespace gddi
