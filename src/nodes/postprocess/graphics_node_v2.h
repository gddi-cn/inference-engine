//
// Created by cc on 2021/11/5.
//

#ifndef __GRAPHICS_NODE_V2_HPP__
#define __GRAPHICS_NODE_V2_HPP__

#include "message_templates.hpp"
#include "modules/cvrelate/graphics.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {

class Graphics_v2 : public node_any_basic<Graphics_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;

public:
    explicit Graphics_v2(std::string name)
        : node_any_basic<Graphics_v2>(std::move(name)),
          graphics_(std::make_shared<graphics::Graphics>()) {
        // bind_simple_property("ttf_file", ttf_file_, "字体文件");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&Graphics_v2::on_cv_image, this);

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~Graphics_v2() override = default;

private:
    void on_setup() override { graphics_->set_font_face(ttf_file_); }
    void on_cv_image(const std::shared_ptr<msgs::cv_frame> &frame);

private:
    std::shared_ptr<graphics::Graphics> graphics_;
    std::string ttf_file_ = "/home/config/NotoSansCJK-Regular.ttc";
};
}// namespace nodes
}// namespace gddi

#endif//__GRAPHICS_NODE_V2_HPP__
