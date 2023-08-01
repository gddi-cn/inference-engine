#include "label_counter_condition_node_v2.h"

namespace gddi {
namespace nodes {

std::map<std::string, std::function<bool(int, int)>> label_counter_condition_ops{
    {"<", std::less<int>()},           {"<=", std::less_equal<int>()}, {">", std::greater<int>()},
    {">=", std::greater_equal<int>()}, {"==", std::equal_to<int>()},   {"!=", std::not_equal_to<int>()}};

void LabelCounterCondition_v2::on_setup() {
    if (label_counter_condition_ops.count(operator_) == 0) { quit_runner_(TaskErrorCode::kInvalidArgument); }
}

void LabelCounterCondition_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    auto back_ext_info = frame->frame_info->ext_info.back();

    int count = 0;
    for (const auto &[idx, target] : back_ext_info.map_target_box) {
        if (back_ext_info.map_class_label.at(target.class_id) == label_) { ++count; }
    }

    frame->frame_info->frame_event_result = label_counter_condition_ops[operator_](count, threshold_);

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi
