
#include "mpp_decoder.h"

#if defined(WITH_RV1126)

#include <cstdint>
#include <rockchip/mpp_packet.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <thread>

#include "rockchip/mpp_frame.h"
#include "rockchip/rk_mpi.h"

namespace av_wrapper {

RgaSURF_FORMAT mppfmt_to_rgafmt(const MppFrameFormat format) {
    switch (format) {
        case MPP_FMT_YUV420SP: return RgaSURF_FORMAT::RK_FORMAT_YCbCr_420_SP;
        case MPP_FMT_YUV420P: return RgaSURF_FORMAT::RK_FORMAT_YCbCr_420_P;
        case MPP_FMT_YUV422P: return RgaSURF_FORMAT::RK_FORMAT_YCbCr_422_P;
        case MPP_FMT_YUV422SP: return RgaSURF_FORMAT::RK_FORMAT_YCbCr_422_SP;
        default: return RgaSURF_FORMAT::RK_FORMAT_UNKNOWN;
    }
}

MppDecoder::MppDecoder() {}

MppDecoder::~MppDecoder() { close_decoder(); }

bool MppDecoder::open_decoder(CodingType type, const int width, const int height) {
    close_decoder();

    // 初始化MPP
    if (mpp_create(&ctx, &mpi) != MPP_OK) {
        spdlog::error("mpp_create failed");
        return false;
    }

    // 配置解码器
    uint32_t need_split = -1;
    if (mpi->control(ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, (MppParam *)&need_split) != MPP_OK) {
        spdlog::error("mpi->control failed");
        return false;
    }

    if (mpp_init(ctx, MPP_CTX_DEC, MPP_VIDEO_CodingAVC) != MPP_OK) {
        spdlog::error("mpp_init failed");
        return false;
    }

    return true;
}

bool MppDecoder::decode(const uint8_t *data, const uint32_t size, const CodecCallback &callback) {
    mpp_packet_init(&packet, const_cast<uint8_t *>(data), size);

    uint32_t pkt_done = 0;

    do {
        if (mpi->decode_put_packet(ctx, packet) == MPP_OK) { pkt_done = 1; }

        if (mpi->decode_get_frame(ctx, &frame) != MPP_OK || !frame) {
            usleep(2000);
            continue;
        }

        if (mpp_frame_get_info_change(frame)) {
            uint32_t width = mpp_frame_get_width(frame);
            uint32_t height = mpp_frame_get_height(frame);
            uint32_t hor_stride = mpp_frame_get_hor_stride(frame);
            uint32_t ver_stride = mpp_frame_get_ver_stride(frame);
            uint32_t buf_size = mpp_frame_get_buf_size(frame);
            MppFrameFormat fmt = mpp_frame_get_fmt(frame);

            spdlog::info("{} decoder require buffer w:h [{}:{}] stride [{}:{}] buf_size {}, fmt: {}", ctx, width,
                         height, hor_stride, ver_stride, buf_size, fmt);

            if (mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, nullptr)) {
                spdlog::error("{} decoder info change ready failed", ctx);
            }

            continue;
        }

        ++image_id;

        if (callback) {
            auto buffer = mpp_frame_get_buffer(frame);
            callback(image_id, (uint8_t *)mpp_buffer_get_ptr(buffer), mpp_frame_get_hor_stride(frame),
                     mpp_frame_get_ver_stride(frame), mppfmt_to_rgafmt(mpp_frame_get_fmt(frame)));
        }

        mpp_frame_deinit(&frame);
        frame = nullptr;
    } while (!pkt_done);

    return true;
}

void MppDecoder::close_decoder() {
    if (mpi) { mpi->reset(ctx); }

    if (ctx) { mpp_destroy(ctx); }

    if (packet) {
        mpp_packet_deinit(&packet);
        free(pkt_buf);
    }
}

}// namespace av_wrapper

#endif