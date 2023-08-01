/**
 * @file aggregation_node_v2.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-04-18
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#ifndef __AGGREGATION_NODE_V2_H__
#define __AGGREGATION_NODE_V2_H__

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class Aggregation_v2 : public node_any_basic<Aggregation_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;

public:
    explicit Aggregation_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&Aggregation_v2::on_cv_image_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }
    ~Aggregation_v2() = default;

protected:
    void on_setup() override;
    void on_cv_image_(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::map<int64_t, std::vector<std::shared_ptr<FrameInfo>>> cache_frame_info_;
    size_t input_endpoint_count_{0};
};
}// namespace nodes
}// namespace gddi

#endif// __AGGREGATION_NODE_V2_H__
