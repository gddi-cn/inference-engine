#ifndef ROI_MEASURE_H_
#define ROI_MEASURE_H_

#include <map>

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

class RoiMeasure : public node_any_basic<RoiMeasure> {
public:
    explicit RoiMeasure(std::string name) : node_any_basic<RoiMeasure>(std::move(name)) {
        bind_simple_property("iof", iof_thres_, "iof");
        bind_simple_property("roi_x1", bind_x1_, "roi_x1");
        bind_simple_property("roi_y1", bind_y1_, "roi_y1");
        bind_simple_property("roi_x2", bind_x2_, "roi_x2");
        bind_simple_property("roi_y2", bind_y2_, "roi_y2");
        bind_simple_property("class_labels", bind_class_labels_, "class_labels");

        register_input_message_handler_(&RoiMeasure::on_image_, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_setup() override {
        std::string delimiter{","};
        roi_region_ = {bind_x1_, bind_y1_, bind_x2_, bind_y2_};

        specific_labels_ = gddi::utils::ssplit(bind_class_labels_, delimiter);
    }

    void on_image_(const std::shared_ptr<msgs::cv_frame> &frame) {

        std::cout << "------------- " << frame->frame_info->frame_idx << " "
                  << frame->frame_info->basic_specific_msg->name() << std::endl;

        frame->frame_info->basic_specific_msg->set_data(frame->frame_info);
        roi_result_ =
            frame->frame_info->basic_specific_msg->roi(roi_region_, specific_labels_, iof_thres_);

        output_result_(frame);
    }

public:
    std::map<std::string, std::vector<int>> roi_result_;

private:
    message_pipe<msgs::cv_frame> output_result_;
    float iof_thres_;
    std::vector<int> roi_region_;
    std::vector<std::string> specific_labels_;
    int bind_x1_;
    int bind_y1_;
    int bind_x2_;
    int bind_y2_;
    std::string bind_class_labels_;
};

}// namespace nodes
}// namespace gddi

#endif// end ROI_MEASURE_H_
