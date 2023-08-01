#ifndef COUNT_NODE_HPP_
#define COUNT_NODE_HPP_

#include <map>

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

class Count : public node_any_basic<Count> {
public:
    explicit Count(std::string name) : node_any_basic<Count>(std::move(name)) {
        register_input_message_handler_(&Count::on_image_, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_image_(const std::shared_ptr<msgs::cv_frame> &frame) {

        std::cout << "------------- " << frame->frame_info->frame_idx << " "
                  << frame->frame_info->basic_specific_msg->name() << std::endl;
        std::cout << "==> ori size: " << frame->frame_info->ext_info.back().vec_target_box.size()
                  << std::endl;

        frame->frame_info->basic_specific_msg->set_data(frame->frame_info);
        count_result_ = frame->frame_info->basic_specific_msg->count();
        // image->basic_specific_msg_->reset_data();
        print_res(count_result_);

        output_result_(frame);
    }

    void print_res(const std::map<std::string, int> &res) {
        for (auto &r : res) { std::cout << r.first << " " << r.second << std::endl; }
    }

public:
    std::map<std::string, int> count_result_;

private:
    message_pipe<msgs::cv_frame> output_result_;
    // std::string save_path_;
};

}// namespace nodes
}// namespace gddi

#endif
