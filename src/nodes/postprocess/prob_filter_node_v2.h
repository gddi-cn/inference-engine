//
// Created by zhdotcai on 2022/11/15.
//

#ifndef __PROB_FILTER_NODE_V2_HPP__
#define __PROB_FILTER_NODE_V2_HPP__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <set>

namespace gddi {
namespace nodes {
class ProbFilter_v2 : public node_any_basic<ProbFilter_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit ProbFilter_v2(std::string name) : node_any_basic<ProbFilter_v2>(std::move(name)) {
        bind_simple_property("box_prob", box_prob_, 0, 1.0, "目标框置信度");
        bind_simple_property("key_point_prob", key_point_prob_, 0, 1.0, "关键点置信度");

        register_input_message_handler_(&ProbFilter_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~ProbFilter_v2() override = default;

private:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float box_prob_ = 0;
    float key_point_prob_ = 0;
};

}// namespace nodes
}// namespace gddi

#endif//__PROB_FILTER_NODE_V2_HPP__
