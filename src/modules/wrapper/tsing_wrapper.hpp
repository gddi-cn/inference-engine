#ifndef __TSING_WRAPPER_HPP__
#define __TSING_WRAPPER_HPP__

#ifdef WITH_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

#include "../mem_pool.hpp"
#include "core/mem/buf_surface_util.h"
#include "types.hpp"
#include <fstream>
#include <map>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <string>
#include <vector>

#include "tscv_operator.h"
#include "tsing_jpeg_encode.h"

namespace gddi {

using ImagePool = gddi::MemPool<AVFrame, int, int>;

namespace image_wrapper {

static bool preview_init = false;
static bool has_init_jpeg = false;
static std::mutex mutex_jpeg;

static TSCV_CROP_S crop_attr;
static bool has_init_crop = false;

inline AVPixelFormat convert_deprecated_format(enum AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P; break;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P; break;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P; break;
        case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P; break;
        default: return format; break;
    }
}

static std::shared_ptr<AVFrame> image_jpeg_dec(const unsigned char *raw_image, size_t dsize) {
    // auto vec_data = std::vector<uchar>(dsize);
    // memcpy(vec_data.data(), raw_image, dsize);

    // auto mat_image = std::make_shared<cv::Mat>();
    // *mat_image = cv::imdecode(vec_data, cv::ImreadModes::IMREAD_COLOR);
    return nullptr;
}

static std::shared_ptr<AVFrame> image_png_dec(const unsigned char *raw_image, size_t dsize) {
    auto vec_data = std::vector<uchar>(dsize);
    memcpy(vec_data.data(), raw_image, dsize);

    auto mat_image = std::make_shared<cv::Mat>();
    *mat_image = cv::imdecode(vec_data, cv::ImreadModes::IMREAD_COLOR);
    return nullptr;
}

static auto image_resize(const std::shared_ptr<AVFrame> &frame, int width, int height) {
    // auto resize_image = cv::Mat(frame->height, frame->width, CV_8UC3);
    // const int dstStride[] = {(int)resize_image.step1()};
    // SwsContext *conversion = sws_getContext(
    //     frame->width, frame->height, convert_deprecated_format((AVPixelFormat)frame->format), frame->width,
    //     frame->height, AVPixelFormat::AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    // sws_scale(conversion, frame->data, frame->linesize, 0, frame->height, &resize_image.data, dstStride);
    // sws_freeContext(conversion);

    return frame;
}

static auto image_crop(const std::shared_ptr<AVFrame> &image, std::map<int, Rect2f> &crop_rects) {
    auto crop_num = crop_rects.size();
    std::map<int, cv::Mat> map_crop_images;

    for (const auto &[idx, rect] : crop_rects) {
        if (!has_init_crop) {
            has_init_crop = 1;

            crop_attr.src_w = image->width;
            crop_attr.src_h = image->height;
            crop_attr.dst_w = rect.width;
            crop_attr.dst_h = rect.height;
            crop_attr.crop_rect.x = rect.x;
            crop_attr.crop_rect.y = rect.y;
            crop_attr.crop_rect.w = rect.width;
            crop_attr.crop_rect.h = rect.height;

            TSCV_CreateVb(image->width, image->height, PIXEL_FORMAT_NV_12, &crop_attr.dst_phy_addr,
                          &crop_attr.dst_vir_addr);
        }

        tscv_vgs_crop_nv12(image.get(), crop_attr);

        map_crop_images[idx] = cv::Mat(rect.height * 3 / 2, rect.width, CV_8UC1);
        memcpy(map_crop_images[idx].data, (char *)crop_attr.dst_vir_addr, rect.width * rect.height * 3 / 2);
    }

    return map_crop_images;
}

static void image_save_as_jpeg(const cv::Mat &image, const std::string &path, const int quality = 85) {
    cv::Mat bgr(image.rows * 2 / 3, image.cols, CV_8UC3);
    cv::cvtColor(image, bgr, cv::COLOR_YUV2BGR_NV12);
    cv::imwrite(path, bgr, std::vector<int>{cv::IMWRITE_JPEG_QUALITY, quality});
}

static cv::Mat image_to_mat(const std::shared_ptr<AVFrame> &image) {
    return cv::Mat(image->height, image->width, CV_8UC3);
}

static std::shared_ptr<AVFrame> mat_to_image(const cv::Mat &image) {
    // auto mat_image = std::make_shared<cv::Mat>(image, cv::Rect(0, 0, image.cols, image.rows));
    return nullptr;
}

static cv::Mat image_to_mat(const std::shared_ptr<cv::Mat> &image) {
    cv::Mat bgr(image->rows * 2 / 3, image->cols, CV_8UC3);
    cv::cvtColor(*image, bgr, cv::COLOR_YUV2BGR_NV12);
    return bgr;
}

static cv::Mat yuv420p_to_bgr888(const AVFrame &src_image) {
    auto dst_image = cv::Mat(src_image.height, src_image.width, CV_8UC3);
    // cv::cvtColor(dst_image, dst_image, cv::COLOR_YUV2BGR_I420);
    return dst_image;
}

static cv::Mat yuv420p_to_bgr888(const cv::Mat &image) {
    cv::Mat bgr(image.rows * 2 / 3, image.cols, CV_8UC3);
    cv::cvtColor(image, bgr, cv::COLOR_YUV2BGR_NV12);
    return bgr;
}

static std::shared_ptr<MemObject<AVFrame>> image_from_avframe(ImagePool &mem_pool,
                                                              const std::shared_ptr<AVFrame> &frame) {
    auto mem_obj = mem_pool.alloc_mem_detach(frame->width, frame->height);

    if (frame->format == AV_PIX_FMT_NV12) {
        mem_obj->data = frame;
    } else {
        mem_obj->data = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *ptr) { av_frame_free(&ptr); });
        mem_obj->data->format = AV_PIX_FMT_NV12;
        mem_obj->data->width = frame->width;
        mem_obj->data->height = frame->height;
        av_frame_get_buffer(mem_obj->data.get(), 1);

        auto sws_ctx = sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width, frame->height,
                                      AV_PIX_FMT_NV12, SWS_BICUBIC, nullptr, nullptr, nullptr);
        sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, mem_obj->data->data,
                  mem_obj->data->linesize);
        sws_freeContext(sws_ctx);
    }

    return mem_obj;
}

}// namespace image_wrapper
}// namespace gddi

#endif