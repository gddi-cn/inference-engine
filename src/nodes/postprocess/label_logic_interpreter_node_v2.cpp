#include "label_logic_interpreter_node_v2.h"
#include "spdlog/spdlog.h"
#include <regex>

namespace gddi {
namespace nodes {

std::string trim(const std::string &str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return (start < end ? std::string(start, end) : "");
}

void LabelLogicInterpreter_v2::on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame) {
    auto &back_ext_info = frame->frame_info->ext_info.back();

    if (!is_valid_expression_) {
        expression_.register_symbol_table(symbol_table_);
        for (auto &[_, value] : back_ext_info.map_class_label) { variables_[trim(value)] = 0; }
        for (auto &[key, value] : variables_) { symbol_table_.add_variable(key.c_str(), value); }
        parser_t parser;
        if (!parser.compile(str_expression_, expression_)) {
            quit_runner_(TaskErrorCode::kInvalidArgument);
            return;
        }
        is_valid_expression_ = true;
    }

    for (const auto &[_, bbox] : back_ext_info.map_target_box) {
        if (variables_.count(back_ext_info.map_class_label.at(bbox.class_id)) > 0) {
            ++variables_[back_ext_info.map_class_label.at(bbox.class_id)];
        }
    }

    frame->frame_info->frame_event_result = expression_.value();

    for (auto &[key, value] : variables_) { value = 0; }

    output_result_(frame);
}

}// namespace nodes
}// namespace gddi