//
// Created by cc on 2021/11/15.
//

#ifndef __AVTOCV_NODE_V2_HPP__
#define __AVTOCV_NODE_V2_HPP__

#include <map>

#ifdef WITH_YUV2BGR
#include <yuv2bgr.h>
#endif

#include "message_templates.hpp"
#include "modules/bm1684_wrapper.hpp"
#include "modules/g_cv_wrapper.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class AvToCv_v2 : public node_any_basic<AvToCv_v2> {
private:
    message_pipe<msgs::cv_frame> output_image_;
#if WITH_BM1684
    std::shared_ptr<bm_handle_t> bm_handle_;
    gddi::MemPool<bm_image, int, int> mem_pool_;
    int dev_id_ = 0;
#else
    gddi::MemPool<cv::Mat, int, int> mem_pool_;
#endif
    std::string task_name_;

public:
    explicit AvToCv_v2(std::string name) : node_any_basic(std::move(name)) {
        bind_simple_property("task_name", task_name_, ngraph::PropAccess::kPrivate);

        register_input_message_handler_(&AvToCv_v2::on_frame_, this);
        output_image_ = register_output_message_<msgs::cv_frame>();
    }

protected:
#if WITH_BM1684
    void on_setup() override {
        bm_handle_ = std::shared_ptr<bm_handle_t>(new bm_handle_t,
                                                  [](bm_handle_t *ptr) { bm_dev_free(*ptr); });
        if (bm_dev_request(bm_handle_.get(), dev_id_) != 0) {
            printf("** failed to request device\n");
            quit_runner_(TaskErrorCode::kAvToCv);
        }

        mem_pool_.register_alloc_callback([bm_handle = this->bm_handle_](bm_image *ptr, int width,
                                                                         int height) {
            auto align_width = width % 64 == 0 ? width : (width / 64 + 1) * 64;
            int stride[] = {align_width, align_width / 2, align_width / 2};
            bm_image_create(*bm_handle, height, width, FORMAT_YUV420P, DATA_TYPE_EXT_1N_BYTE, ptr);
            bm_image_alloc_dev_mem_heap_mask(*ptr, 6);
        });

        mem_pool_.register_free_callback([](bm_image *ptr) { bm_image_destroy(*ptr); });
    }
#endif

    void on_frame_(const std::shared_ptr<msgs::av_frame> &av_frame) {
        auto frame = std::make_shared<msgs::cv_frame>(task_name_);
#if defined(WITH_BM1684)
        auto mem_obj = mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(
            nullptr, av_frame->frame_->width, av_frame->frame_->height);
        image_wrapper::bm_image_from_frame(*av_frame->frame_, *mem_obj->data);
        frame->frame_info = std::make_shared<nodes::FrameInfo>(av_frame->frame_idx_, mem_obj);
#elif defined(WITH_MLU2XX)
        // TODO: AvFrame -> PackagePtr
#else
        auto mem_obj =
            mem_pool_.alloc_mem_detach(av_frame->frame_->width, av_frame->frame_->height);
        if (av_frame->frame_->format == AV_PIX_FMT_CUDA) {
#ifdef WITH_YUV2BGR
            cv::cuda::GpuMat image;
            image.create(av_frame->frame_->height / 10 * 10, av_frame->frame_->width / 10 * 10,
                         CV_8UC3);
            image.step = av_frame->frame_->width * 3;
            nv12_to_bgr(av_frame->frame_->data[0], image.data,
                        av_frame->frame_->width * av_frame->frame_->height,
                        av_frame->frame_->height, av_frame->frame_->width,
                        av_frame->frame_->linesize[0]);

            if (image.empty()) { throw std::runtime_error("gpumat is empty"); }

            image.download(*mem_obj->data);
#else
            AVFrame *hw_frame = nullptr;
            if (!(hw_frame = av_frame_alloc())) {
                throw std::runtime_error("failed to alloc frame");
            }

            if (av_hwframe_transfer_data(hw_frame, av_frame->frame_, 0) < 0) {
                throw std::runtime_error("Error transferring the data to system memory");
            }
            av_frame_copy_props(hw_frame, av_frame->frame_);
            *mem_obj->data = avframe_to_cvmat(hw_frame);
            av_frame_free(&hw_frame);
#endif
        } else {
            *mem_obj->data = avframe_to_cvmat(av_frame->frame_);
        }
        frame->frame_info = std::make_shared<nodes::FrameInfo>(av_frame->frame_idx_, mem_obj);
#endif
        output_image_(frame);
    }

    cv::Mat avframe_to_cvmat(AVFrame *frame) {
        cv::Mat cv_frame;

        int width = frame->width;
        int height = frame->height;

        auto dst_frame = av_frame_alloc();
        dst_frame->format = AV_PIX_FMT_BGR24;
        dst_frame->width = frame->width;
        dst_frame->height = frame->height;
        int buf_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width, frame->height, 1);
        if (buf_size < 0) { throw std::runtime_error("fail to calc frame buffer size"); }
        AV_RUNTIME_ERROR(av_frame_get_buffer(dst_frame, 1), "fail to alloc buffer");

        SwsContext *conversion = sws_getContext(
            width, height, _convert_deprecated_format((AVPixelFormat)frame->format), width, height,
            AVPixelFormat::AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
        sws_scale(conversion, frame->data, frame->linesize, 0, height, dst_frame->data,
                  dst_frame->linesize);
        sws_freeContext(conversion);
        cv_frame =
            cv::Mat(height, width, CV_8UC3, dst_frame->data[0], dst_frame->linesize[0]).clone();
        av_frame_free(&dst_frame);

        return cv_frame;
    }

    AVPixelFormat _convert_deprecated_format(enum AVPixelFormat format) {
        switch (format) {
            case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P; break;
            case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P; break;
            case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P; break;
            case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P; break;
            default: return format; break;
        }
    }
};

};// namespace nodes

}// namespace gddi

#endif//__AVTOCV_NODE_V2_HPP__
