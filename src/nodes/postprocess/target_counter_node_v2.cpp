#include "target_counter_node_v2.h"
#include "node_msg_def.h"
#include "spdlog/spdlog.h"

#if defined(WITH_NVIDIA)
#include <opencv2/cudaarithm.hpp>
#endif

namespace gddi {
namespace nodes {

void TargetCounter_v2::on_setup() {
    counting_logic_obj_ = nlohmann::json::parse(counting_logic_);
    for (auto &item : counting_logic_obj_) {
        auto operator_v = item["operator"].get<std::string>();
        auto output_label = item["output_label"].get<std::string>();

        target_counts_[output_label] = 0;
        if (operator_v == "INDEX") {
            index_label_ = output_label;
            ++target_counts_[output_label];
        } else {
            labele_counts_[output_label] = 0;
        }
    }
}

void TargetCounter_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    std::set<std::string> target_labels;
    for (const auto &[target_id, item] : frame->frame_info->ext_info.back().map_target_box) {
        target_labels.insert(frame->frame_info->ext_info.back().map_class_label.at(item.class_id));
    }

    if (frame->check_report_callback_(frame->frame_info->ext_info) == FrameType::kReport) {
        frame->frame_info->ext_info.back().target_counts = target_counts_;

        for (auto &[key, value] : target_counts_) {
            if (key == index_label_) {
                ++value;
            } else {
                value = 0;
            }
        }
    } else {
        for (auto &item : counting_logic_obj_) {
            auto input_labels = item["input_labels"].get<std::vector<std::string>>();
            auto operator_v = item["operator"].get<std::string>();
            auto output_label = item["output_label"].get<std::string>();

            if (operator_v == "AND") {
                int index = 0;
                for (const auto &item : input_labels) {
                    if (target_labels.count(item) > 0) { ++index; }
                }
                if (index == input_labels.size()) {
                    ++target_counts_[output_label];
                    break;
                }
            } else if (operator_v == "OR") {
                int index = 0;
                for (const auto &item : input_labels) {
                    if (target_labels.count(item) > 0) { ++index; }
                }
                if (index > 0) {
                    ++target_counts_[output_label];
                    break;
                }
            } else if (operator_v == "EMPTY") {
                if (target_labels.empty()) {
                    ++target_counts_[output_label];
                    break;
                }
            } else if (operator_v == "NOT-EXIST") {
                if (target_labels.count(input_labels[0]) == 0) {
                    ++target_counts_[output_label];
                    break;
                }
            }
        }

        for (auto &[label, count] : target_counts_) {
            spdlog::info("=========================== label: {}, count: {}", label, count);
        }

        frame->frame_info->ext_info.back().target_counts = target_counts_;
    }

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi
