#ifndef DISTANCE_NODE_HPP_
#define DISTANCE_NODE_HPP_

#include <cassert>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {

class DistanceMeasure : public node_any_basic<DistanceMeasure> {
public:
    explicit DistanceMeasure(std::string name) : node_any_basic<DistanceMeasure>(std::move(name)) {
        bind_simple_property("distance_thres", distance_thres_, "distance_thres");
        bind_simple_property("class_labels", bind_class_labels_, "class_labels");

        register_input_message_handler_(&DistanceMeasure::on_image_, this);
        output_result_ = register_output_message_<msgs::cv_frame>();
    }

protected:
    void on_setup() override {
        std::string delimiter{","};
        specific_label_pairs_ = gddi::utils::ssplit(bind_class_labels_, delimiter);
        _pair_class(specific_label_pairs_);
    }

    void on_image_(const std::shared_ptr<msgs::cv_frame> &frame) {

        std::cout << "-------- " << frame->frame_info->frame_idx << " "
                  << frame->frame_info->basic_specific_msg->name() << std::endl;

        frame->frame_info->basic_specific_msg->set_data(frame->frame_info);
        distance_result_ = frame->frame_info->basic_specific_msg->distance(specific_label_pairs_,
                                                                           distance_thres_, false);

        output_result_(frame);
    }

    void _pair_class(const std::vector<std::string> &labels) {
        if (labels.size() == 1) {
            if (labels[0].size() != 0) {
                specific_label_pairs_.push_back(labels[0]);
            } else {
                assert(false && "class_labels should be specified");
            }
        }
        assert((labels.size() % 2 == 0) && "class_labels should be pair");
    }

public:
    std::map<std::pair<std::string, std::string>, std::vector<std::pair<int, int>>>
        distance_result_;

private:
    message_pipe<msgs::cv_frame> output_result_;
    float distance_thres_;
    std::vector<std::string> specific_label_pairs_;
    std::string bind_class_labels_;
};

}// namespace nodes
}// namespace gddi

#endif// end DISTANCE_NODE_HPP_
