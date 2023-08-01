//
// Created by zhdotcai on 4/21/22.
//

#ifndef INFERENCE_ENGINE_OPENCV_DECODER_NODE_V2_H
#define INFERENCE_ENGINE_OPENCV_DECODER_NODE_V2_H

#include "basic_logs.hpp"
#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"
#include <map>
#include <opencv2/core/cuda.hpp>

namespace gddi {
namespace nodes {
class CvDecoder_v2 : public node_any_basic<CvDecoder_v2> {
private:
    message_pipe<msgs::av_decode_open> output_open_decoder_;
    message_pipe<msgs::cv_frame> output_cv_frame_;

public:
    explicit CvDecoder_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("input_url", input_url_, "输入源地址");
        bind_simple_property("input_type", input_type_, ngraph::PropAccess::kProtected);

        bind_simple_property("task_name", task_name_, ngraph::PropAccess::kPrivate);

        output_open_decoder_ = register_output_message_<msgs::av_decode_open>();
        output_cv_frame_ = register_output_message_<msgs::cv_frame>();
    }

    ~CvDecoder_v2() {
        running_ = false;
        if (thread_handle_.joinable()) { thread_handle_.join(); }
    }

protected:
    void on_setup() override {
#ifdef WITH_BM1684
        mem_pool_.register_free_callback([](bm_image *ptr) { bm_image_destroy(*ptr); });
#endif
        thread_handle_ = std::thread([this]() {
            while (running_) {
                if (capture_.isOpened()) {
                    cv::Mat frame;
                    if (capture_.read(frame)) {
                        on_frame(frame_idx_++, frame);
                    } else {
                        capture_.release();
                    }
                }
#ifdef WITH_BM1684
                else if (capture_.open(input_url_, cv::CAP_FFMPEG)) {
                    capture_.set(cv::CAP_PROP_OUTPUT_YUV, PROP_TRUE);
                }
#else
                else if (capture_.open(input_url_)) {
                }
#endif
                else {
                    spdlog::error("Failed to open input url: {}", input_url_);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        });
    }

    void on_frame(const int64_t frame_idx, cv::Mat &cv_frame) {
        auto frame =
            std::make_shared<msgs::cv_frame>(task_name_, gddi::TaskType::kCamera, capture_.get(cv::CAP_PROP_FPS));
#if defined(WITH_BM1684)
        auto mem_obj = mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(nullptr, FORMAT_RGB_PACKED, cv_frame.cols,
                                                                            cv_frame.rows, false);
        bm_image_from_mat(*image_wrapper::bm_handle, cv_frame, *mem_obj->data);
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
        auto mem_obj = mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(nullptr, CNCODEC_PIX_FMT_NV12,
                                                                            cv_frame.cols, cv_frame.rows);
#elif defined(WITH_NVIDIA)
        auto mem_obj = mem_pool_.alloc_mem_detach(cv_frame.cols, cv_frame.rows);
        mem_obj->data->upload(cv_frame);
#elif defined(WITH_MLU370)
        auto mem_obj = mem_pool_.alloc_mem_detach();
#else
        auto mem_obj = mem_pool_.alloc_mem_detach(cv_frame.cols, cv_frame.rows);
#endif
        frame->frame_info = std::make_shared<nodes::FrameInfo>(frame_idx, mem_obj);
        output_cv_frame_(frame);
    }

private:
    bool running_ = true;
    std::thread thread_handle_;

    ImagePool mem_pool_;

    int64_t frame_idx_{};
    cv::VideoCapture capture_;

    std::string input_url_;
    std::string input_type_;
    int loop_time_ = INT32_MAX;

    std::string task_name_;
};

}// namespace nodes

}// namespace gddi

#endif//INFERENCE_ENGINE_OPENCV_DECODER_NODE_V2_H
