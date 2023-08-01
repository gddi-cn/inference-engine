//
// Created by cc on 2021/11/5.
//

#ifndef __INFERENCE_NODE_V2_H__
#define __INFERENCE_NODE_V2_H__

#include "blockingconcurrentqueue.h"
#include "message_templates.hpp"
#include "modules/algorithm/algo_factory.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include <thread>

namespace gddi {
namespace nodes {

class Inference_v2 : public node_any_basic<Inference_v2> {
protected:
    message_pipe<msgs::cv_frame> output_result_;
    algo::ModParms parms_;

public:
    explicit Inference_v2(std::string name) : node_any_basic<Inference_v2>(std::move(name)) {
#ifdef WITH_BM1684
        bind_simple_property("stream_id", parms_.stream_id, "实例 ID", ngraph::PropAccess::kPrivate);
#endif
        bind_simple_property("mod_iter_id", parms_.mod_id, "模型ID");
        bind_simple_property("mod_name", parms_.mod_name, "模型名称");
        bind_simple_property("mod_path", parms_.mod_path, "模型文件");
        bind_simple_property("mod_labels", mod_label_objs_, "标签列表");
        bind_simple_property("lib_paths", parms_.lib_paths, "特征库路径");
        bind_simple_property("best_threshold", parms_.mod_thres, "模型阈值", ngraph::PropAccess::kProtected);
        bind_simple_property("frame_rate", parms_.frame_rate, "推理帧率");
        bind_simple_property("instances", parms_.instances, "推理实例数");
        bind_simple_property("batch_size", parms_.batch_size, "批量数");

        bind_simple_flags("support_preview", true);

        register_input_message_handler_(&Inference_v2::on_cv_frame, this);

        output_result_ = register_output_message_<msgs::cv_frame>();
    }

    ~Inference_v2();

private:
    void on_setup() override;
    void on_cv_frame(const std::shared_ptr<msgs::cv_frame> &frame);

    nodes::FrameExtInfo parser_output(const AlgoType type, const std::vector<algo::AlgoOutput> &vec_output);
    nodes::FrameExtInfo parser_output(const AlgoType type, const FrameExtInfo &back_ext_info,
                                      const std::map<int, std::vector<algo::AlgoOutput>> &outputs);

private:
    bool active{true};
    std::vector<std::thread> thread_handle_;
    nlohmann::json mod_label_objs_;

    std::map<int, std::string> map_class_label_;
    std::map<int, std::vector<int>> map_class_color_;

    int64_t infer_frame_idx_{1};

    moodycamel::BlockingConcurrentQueue<std::shared_ptr<msgs::cv_frame>> cache_frames_;
};
}// namespace nodes
}// namespace gddi

#endif//__INFERENCE_NODE_V2_H__
