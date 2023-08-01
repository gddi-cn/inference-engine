//
// Created by zhdotcai on 4/21/22.
//

#ifndef INFERENCE_ENGINE_MEDIA_DECODER_NODE_V2_H
#define INFERENCE_ENGINE_MEDIA_DECODER_NODE_V2_H

#include "basic_logs.hpp"
#include "message_templates.hpp"
#include "modules/codec/decode_video_v3.h"
#include "modules/codec/demux_stream_v3.h"
#include "modules/codec/filter_video_v3.h"
#include "modules/codec/mpp_decoder.h"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <chrono>
#include <map>

namespace gddi {
namespace nodes {
class MediaDecoder_v2 : public node_any_basic<MediaDecoder_v2> {
private:
    message_pipe<msgs::av_decode_open> output_open_decoder_;
    message_pipe<msgs::cv_frame> output_cv_frame_;

public:
    explicit MediaDecoder_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("input_url", input_url_, "输入源地址");
        bind_simple_property("input_type", input_type_, ngraph::PropAccess::kProtected);

        //        bind_simple_property("enable_acc", enable_acc_);  // 暂不处理音频
        bind_simple_property("task_name", task_name_, ngraph::PropAccess::kPrivate);

        output_open_decoder_ = register_output_message_<msgs::av_decode_open>();
        output_cv_frame_ = register_output_message_<msgs::cv_frame>();
    }

    ~MediaDecoder_v2() { demuxer_.reset(); }

protected:
    void on_setup() override;

    void open_demuxer();
    void open_decoder(const std::shared_ptr<AVCodecParameters> &codecpar);

private:
    std::string task_name_;
    std::string input_url_;
    std::string input_type_;
    bool enable_acc_ = false;

    TaskType task_type_ = TaskType::kCamera;
    ImagePool mem_pool_;
    AVHWDeviceType hw_type_ = AV_HWDEVICE_TYPE_NONE;

    double frame_rate_{25};
    std::chrono::system_clock::time_point timestamp_;
    std::shared_ptr<av_wrapper::Demuxer_v3> demuxer_;

#if defined(WITH_RV1126)
    std::shared_ptr<av_wrapper::MppDecoder> decoder_;
#else
    std::shared_ptr<av_wrapper::Decoder_v3> decoder_;
#endif
};

}// namespace nodes

}// namespace gddi

#endif//INFERENCE_ENGINE_MEDIA_DECODER_NODE_V2_H
