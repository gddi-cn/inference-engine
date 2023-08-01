//
// Created by zhdotcai on 2022/10/18.
//

#ifndef __MSG_SUBSCRIBE_NODE_V2_H__
#define __MSG_SUBSCRIBE_NODE_V2_H__

#include <condition_variable>
#include <memory>
#include <queue>

#include "message_templates.hpp"
#include "modules/network/zmq_socket.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {

class MsgSubscribe_v2 : public node_any_basic<MsgSubscribe_v2> {
public:
    explicit MsgSubscribe_v2(std::string name) : node_any_basic<MsgSubscribe_v2>(std::move(name)) {
        bind_simple_property("every_frame", every_frame_, "每帧都推送");
        bind_simple_property("time_interval", time_interval_, "时间间隔");

        register_input_message_handler_(&MsgSubscribe_v2::on_cv_image, this);
    }

private:
    void on_setup() override;
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

    bool frame_info_to_string(const std::string &task_name, const std::string &event_id,
                              const std::shared_ptr<nodes::FrameInfo> &frame, std::string &buffer);

private:
    bool every_frame_{false};// 每帧都推送
    int time_interval_ = 0;  // 时间间隔
    time_t last_event_time_; // 上一次发送时间
};
}// namespace nodes
}// namespace gddi

#endif//__MSG_SUBSCRIBE_NODE_V2_H__
