//
// Created by cc on 2021/11/15.
//

#ifndef __CVTOAV_NODE_V2_HPP__
#define __CVTOAV_NODE_V2_HPP__

#include <map>

#include "message_templates.hpp"
#include "node_any_basic.hpp"
#include "node_msg_def.h"
#include "utils.hpp"

namespace gddi {
namespace nodes {
class CvToAv_v2 : public node_any_basic<CvToAv_v2> {
private:
    message_pipe<msgs::av_frame> output_image_;

public:
    explicit CvToAv_v2(std::string name) : node_any_basic(std::move(name)) {
        register_input_message_handler_(&CvToAv_v2::on_image_, this);
        output_image_ = register_output_message_<msgs::av_frame>();
    }

protected:
    void on_image_(const std::shared_ptr<msgs::cv_frame> &frame) {
        auto frame_info = frame->frame_info;
        auto avframe = std::shared_ptr<AVFrame>(av_frame_alloc(),
                                                [](AVFrame *frame) { av_frame_free(&frame); });

        // #ifdef WITH_BM1684
        //                 auto cvt_image = new bm_image;
        //                 bmcv_rect_t rect{ 0, 0, 1920, 1080};
        //                 bm_image_create(*image->info_->bm_handle,
        //                 rect.crop_h, rect.crop_w, FORMAT_YUV420P,
        //                 DATA_TYPE_EXT_1N_BYTE, cvt_image);
        //                 bm_image_alloc_dev_mem_heap_mask(*cvt_image, 6);
        //                 if
        //                 (bmcv_image_vpp_convert(*image->info_->bm_handle,
        //                 1, *image->info_->bm_target_frame, cvt_image,
        //                 &rect) != BM_SUCCESS)
        //                 {
        //                     bm_image_destroy(*cvt_image);
        //                     free(cvt_image);
        //                     throw std::runtime_error("Failed to resize
        //                     image");
        //                 }
        //                 bm_image_to_avframe(*image->info_->bm_handle,
        //                 cvt_image, avframe.get());
        // #else
        auto &target_frame = frame_info->tgt_frame;

        auto buffer =
            std::shared_ptr<uint8_t>((uint8_t *)av_malloc(av_image_get_buffer_size(
                                         AV_PIX_FMT_NV12, target_frame.cols, target_frame.rows, 1)),
                                     [](uint8_t *buf) { av_free(buf); });

        avframe->width = target_frame.cols;
        avframe->height = target_frame.rows;
        avframe->format = AV_PIX_FMT_NV12;
        int cvLinesizes[1];
        cvLinesizes[0] = target_frame.step1();

        av_image_fill_arrays(avframe->data, avframe->linesize, buffer.get(), AV_PIX_FMT_NV12,
                             avframe->width, avframe->height, 1);
        SwsContext *conversion =
            sws_getContext(target_frame.cols, target_frame.rows, AV_PIX_FMT_BGR24, avframe->width,
                           avframe->height, AV_PIX_FMT_NV12, SWS_FAST_BILINEAR, NULL, NULL, NULL);
        sws_scale(conversion, &target_frame.data, cvLinesizes, 0, target_frame.rows, avframe->data,
                  avframe->linesize);
        sws_freeContext(conversion);
        // #endif
        output_image_(std::make_shared<msgs::av_frame>(frame_info->frame_idx, avframe.get()));
    }
};
}// namespace nodes

}// namespace gddi

#endif//__CVTOAV_NODE_V2_HPP__
