//
// Created by cc on 2021/11/5.
//

#ifndef __CLASSIFICATION_NODE_V2_H__
#define __CLASSIFICATION_NODE_V2_H__

#include "message_templates.hpp"
#include "modules/algorithm/algo_factory.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"

namespace gddi {
namespace nodes {
class Classifier_v2 : public node_any_basic<Classifier_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;
    algo::ModParms parms_;

public:
    explicit Classifier_v2(std::string name) : node_any_basic<Classifier_v2>(std::move(name)) {
        bind_simple_property("mod_iter_id", parms_.mod_id, "模型ID", ngraph::PropAccess::kProtected);
        bind_simple_property("mod_name", parms_.mod_name, "模型名称", ngraph::PropAccess::kProtected);
        bind_simple_property("mod_path", parms_.mod_path, "模型文件", ngraph::PropAccess::kProtected);
        bind_simple_property("mod_labels", mod_label_objs_, "标签列表", ngraph::PropAccess::kProtected);
        bind_simple_property("best_threshold", parms_.mod_thres, "模型阈值", ngraph::PropAccess::kProtected);
        bind_simple_property("frame_rate", parms_.frame_rate, "推理帧率");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&Classifier_v2::on_cv_frame, this);

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~Classifier_v2() override = default;

private:
    void on_setup();

    void on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame);

    nodes::FrameExtInfo parser_classification_output(const std::vector<algo::AlgoOutput> &vec_output);
    nodes::FrameExtInfo parser_classification_output(const std::map<int, std::vector<algo::AlgoOutput>> &outputs);

private:
    nlohmann::json mod_label_objs_;

    std::map<int, std::string> map_class_label_;
    std::map<int, std::vector<int>> map_class_color_;

    moodycamel::ConcurrentQueue<std::shared_ptr<msgs::cv_frame>> que_frame_;

    algo::InferCallback infer_callback_;
    std::unique_ptr<algo::AbstractAlgo> classifier_;
};
}// namespace nodes
}// namespace gddi

#endif//__CLASSIFICATION_NODE_V2_H__
