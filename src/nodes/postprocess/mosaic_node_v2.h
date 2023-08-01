//
// Created by cc on 2022/11/16.
//

#ifndef __MOSAIC_NODE_V2_HPP__
#define __MOSAIC_NODE_V2_HPP__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <set>

namespace gddi {
namespace nodes {
class Mosaic_v2 : public node_any_basic<Mosaic_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit Mosaic_v2(std::string name) : node_any_basic<Mosaic_v2>(std::move(name)) {
        bind_simple_property("check_bbox", check_bbox_, "检测框脱敏");
        bind_simple_property("check_key_point", check_key_point_, "姿态人脸脱敏");

        register_input_message_handler_(&Mosaic_v2::on_cv_image, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~Mosaic_v2() override = default;

private:
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    bool check_bbox_{false};
    bool check_key_point_{false};
};

}// namespace nodes
}// namespace gddi

#endif//__MOSAIC_NODE_V2_HPP__
