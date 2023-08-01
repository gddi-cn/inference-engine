/**
 * @file label_logic_interpreter_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-06-29
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#pragma once

#include "exprtk.hpp"
#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <vector>

namespace gddi {
namespace nodes {

typedef exprtk::symbol_table<float> symbol_table_t;
typedef exprtk::expression<float> expression_t;
typedef exprtk::parser<float> parser_t;

class LabelLogicInterpreter_v2 : public node_any_basic<LabelLogicInterpreter_v2> {
private:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit LabelLogicInterpreter_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("expression", str_expression_, "标签");

        register_input_message_handler_(&LabelLogicInterpreter_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }
    ~LabelLogicInterpreter_v2() = default;

protected:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

    bool parse_expression(const std::string &expression);

private:
    std::string str_expression_;
    std::unordered_map<std::string, float> variables_;

    bool is_valid_expression_{false};
    symbol_table_t symbol_table_;
    expression_t expression_;
};

}// namespace nodes
}// namespace gddi