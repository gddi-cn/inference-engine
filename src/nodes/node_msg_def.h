#ifndef __NODE_MSG_DEF_H__
#define __NODE_MSG_DEF_H__

#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_struct_def.h"

// WARNING: 模型解密 salt 不可改
#define MOD_SALT "inference-engine"

namespace gddi {
namespace nodes {
namespace msgs {
class av_packet : public ngraph::Message {
public:
    explicit av_packet(uint64_t idx, const AVPacket *packet) : pkt_idx_(idx), packet_(av_packet_clone(packet)) {}
    ~av_packet() override { av_packet_free(&packet_); }

    uint64_t pkt_idx_;
    AVPacket *packet_;

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(packet_); }
};

class av_frame : public ngraph::Message {
public:
    explicit av_frame(uint64_t idx, const AVFrame *frame) : frame_idx_(idx), frame_(av_frame_clone(frame)) {}
    ~av_frame() override { av_frame_free(&frame_); }

    int64_t frame_idx_;
    AVFrame *frame_;

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(frame_); }
};

class av_stream_open : public ngraph::Message {
public:
    explicit av_stream_open(const AVCodecParameters *codec_parameters, double fps, AVRational time_base,
                            AVRational r_frame_rate)
        : codec_parameters_(avcodec_parameters_alloc()), fps_(fps), time_base_(time_base), r_frame_rate_(r_frame_rate) {
        avcodec_parameters_copy(codec_parameters_, codec_parameters);
    }
    ~av_stream_open() override { avcodec_parameters_free(&codec_parameters_); }
    AVCodecParameters *codec_parameters_;
    double fps_;
    AVRational time_base_;
    AVRational r_frame_rate_;

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(codec_parameters_); }
};

class av_decode_open : public ngraph::Message {
public:
    explicit av_decode_open(const AVCodecParameters *codec_parameters) : codec_parameters_(avcodec_parameters_alloc()) {
        avcodec_parameters_copy(codec_parameters_, codec_parameters);
    }
    ~av_decode_open() override { avcodec_parameters_free(&codec_parameters_); }
    AVCodecParameters *codec_parameters_;

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(codec_parameters_); }
};

class cv_frame : public ngraph::Message {
public:
    explicit cv_frame(std::string const &name, TaskType const &type = TaskType::kCamera, float f_rate = 25.0,
                      FrameType f_type = FrameType::kBase)
        : task_name(name), task_type(type), video_frame_rate(f_rate), infer_frame_rate(f_rate), frame_type(f_type) {}
    explicit cv_frame(std::shared_ptr<cv_frame> const &other)
        : task_name(other->task_name), task_type(other->task_type) {
        this->frame_type = other->frame_type;
        this->video_frame_rate = other->video_frame_rate;
        this->infer_frame_rate = other->infer_frame_rate;
        this->response_callback_ = other->response_callback_;
        this->check_report_callback_ = other->check_report_callback_;
        this->frame_info = std::make_shared<FrameInfo>(other->frame_info);
    }

    std::string task_name;// 任务 ID
    TaskType task_type;   // 任务 Type

    std::function<void(const nlohmann::json &, const bool)> response_callback_;
    std::function<FrameType(const std::vector<FrameExtInfo> &)> check_report_callback_{
        [](const std::vector<FrameExtInfo> &) { return FrameType::kBase; }};

    FrameType frame_type;                        // 帧类型
    float video_frame_rate;                      // 视频帧率
    float infer_frame_rate;                      // 推理帧率
    std::shared_ptr<nodes::FrameInfo> frame_info;// 帧信息结构

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(task_name); }
};

class msg_hello : public ngraph::Message {
public:
    explicit msg_hello(std::string &info) : info_(std::move(info)) {}
    ~msg_hello() override = default;

    std::string info_;

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(info_); }
};

#if defined(WITH_OPENCV)
class msg_pose : public ngraph::Message {
public:
    explicit msg_pose(cv::Mat &img, std::vector<Rect> &bbox) : img_(std::move(img)), bbox_(std::move(bbox)) {}

    ~msg_pose() override = default;

    cv::Mat img_;
    std::vector<Rect> bbox_;
    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return "msg_pose"; }
};
#endif

class av_encode_open : public ngraph::Message {
public:
    explicit av_encode_open(const AVCodecParameters *codec_parameters, const AVRational &time_base,
                            const AVRational &framerate)
        : codec_parameters_(avcodec_parameters_alloc()), time_base_(time_base), framerate_(framerate) {
        avcodec_parameters_copy(codec_parameters_, codec_parameters);
    }
    ~av_encode_open() override { avcodec_parameters_free(&codec_parameters_); }
    AVCodecParameters *codec_parameters_;
    AVRational time_base_;
    AVRational framerate_;

    std::string name() const override { return utils::get_class_name(this); }
    std::string to_string() const override { return utils::fmts(codec_parameters_); }
};
}// namespace msgs
}// namespace nodes
}// namespace gddi

#endif