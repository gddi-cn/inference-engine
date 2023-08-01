//
// Created by cc on 2021/11/5.
//

#ifndef __DETECTION_NODE_V2_H__
#define __DETECTION_NODE_V2_H__

#include "message_templates.hpp"
#include "modules/algorithm/algo_factory.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <future>
#include <map>
#include <mutex>

namespace gddi {
namespace nodes {
class Detection_v2 : public node_any_basic<Detection_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;
    algo::ModParms parms_;

public:
    explicit Detection_v2(std::string name) : node_any_basic<Detection_v2>(std::move(name)) {
#ifdef WITH_BM1684
        bind_simple_property("stream_id", parms_.stream_id, "实例 ID", ngraph::PropAccess::kPrivate);
#endif
        bind_simple_property("mod_iter_id", parms_.mod_id, "模型ID");
        bind_simple_property("mod_name", parms_.mod_name, "模型名称");
        bind_simple_property("mod_path", parms_.mod_path, "检测模型文件");
        bind_simple_property("mod_labels", mod_label_objs_, "标签列表");
        bind_simple_property("best_threshold", parms_.mod_thres, "模型阈值", ngraph::PropAccess::kProtected);
        bind_simple_property("frame_rate", parms_.frame_rate, "推理帧率");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&Detection_v2::on_cv_frame, this);

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

private:
    void on_setup() override;
    void on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame);
    nodes::FrameExtInfo parser_detection_output(const std::vector<algo::AlgoOutput> &vec_output);
    nodes::FrameExtInfo parser_detection_output(const std::map<int, std::vector<algo::AlgoOutput>> &outputs);

private:
    nlohmann::json mod_label_objs_;

    std::map<int, std::string> map_class_label_;
    std::map<int, std::vector<int>> map_class_color_;

    std::mutex cache_mtx_;
    std::map<int64_t, std::shared_ptr<msgs::cv_frame>> cache_frames_;

    algo::InferCallback infer_callback_;
    std::unique_ptr<algo::AbstractAlgo> detecter_;

    std::future<bool> future_;
};
}// namespace nodes
}// namespace gddi

#endif//__DETECTION_NODE_V2_H__
