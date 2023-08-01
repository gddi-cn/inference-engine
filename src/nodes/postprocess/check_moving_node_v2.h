/**
 * @file perspective_transform_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-05-15
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#ifndef _PERPECTIVE_TRANSFORM_NODE_V2_H__
#define _PERPECTIVE_TRANSFORM_NODE_V2_H__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <vector>

namespace gddi {
namespace nodes {

class CheckMoving_v2 : public node_any_basic<CheckMoving_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit CheckMoving_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("threshold", threshold_, "阈值");

        register_input_message_handler_(&CheckMoving_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~CheckMoving_v2() = default;

protected:
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    float threshold_{0.5};

    std::map<int, BoxInfo> prev_bbox_;
};

}// namespace nodes
}// namespace gddi

#endif// _PERPECTIVE_TRANSFORM_NODE_V2_H__
