/**
 * @file dial_plate_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-04-05
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#ifndef __DIAL_PLATE_NODE_V2_H__
#define __DIAL_PLATE_NODE_V2_H__

#include "message_templates.hpp"
#include "modules/postprocess/angle_calculator.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {

class DialPlate_v2 : public node_any_basic<DialPlate_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit DialPlate_v2(std::string name) : node_any_basic<DialPlate_v2>(std::move(name)) {
        register_input_message_handler_(&DialPlate_v2::on_cv_frame, this);

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

private:
    void on_setup() override;
    void on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::unique_ptr<AngleCalculator> calculator_;
};

}// namespace nodes
}// namespace gddi

#endif
