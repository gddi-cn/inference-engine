//
// Created by cc on 2022/03/15.
//

#ifndef __BOX_FILTER_NODE_V2_HPP__
#define __BOX_FILTER_NODE_V2_HPP__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <set>

namespace gddi {
namespace nodes {
class BoxFilter_v2 : public node_any_basic<BoxFilter_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit BoxFilter_v2(std::string name) : node_any_basic<BoxFilter_v2>(std::move(name)) {
        bind_simple_property("min_width", min_width_, 0, 4096, "最小目标宽");
        bind_simple_property("max_width", max_width_, 0, 4096, "最大目标宽");
        bind_simple_property("min_height", min_height_, 0, 0, "最小目标高");
        bind_simple_property("max_height", max_height_, 0, 2160, "最大目标高");
        bind_simple_property("min_area", min_area_, 0, 4096 * 2160, "最小目标面积");
        bind_simple_property("max_area", max_area_, 0, 4096 * 2160, "最大目标面积");
        bind_simple_property("min_count", min_count_, 0, INT_MAX, "最少目标数");
        bind_simple_property("max_count", max_count_, 0, INT_MAX, "最多目标数");
        bind_simple_property("box_prob", box_prob_, 0, 1.0, "目标置信度");
        bind_simple_property("box_labels", vec_box_labels_, "目标标签");

        register_input_message_handler_(&BoxFilter_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~BoxFilter_v2() override = default;

private:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    int min_width_ = 0;
    int max_width_ = 4096;
    int min_height_ = 0;
    int max_height_ = 2160;
    int min_area_ = 0;
    int max_area_ = 4096 * 2160;
    int min_count_ = 0;
    int max_count_ = INT_MAX;
    float box_prob_ = 0;
    std::set<std::string> vec_box_labels_;
};

}// namespace nodes
}// namespace gddi

#endif//__BOX_FILTER_NODE_V2_HPP__
