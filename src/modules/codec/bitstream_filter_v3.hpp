#ifndef __BITSTRAM_FILTER_V3_HPP_
#define __BITSTRAM_FILTER_V3_HPP_

extern "C" {
#include <libavcodec/version.h>
#include <libavformat/avformat.h>
#if LIBAVCODEC_VERSION_MAJOR == 60
#include <libavcodec/bsf.h>
#endif
}

#include <memory>
#include <string.h>

namespace av_wrapper {

using AvPacketCallback = std::function<void(const std::shared_ptr<AVPacket> &)>;

class BitStreamFilter_v3 {
public:
    ~BitStreamFilter_v3() {
        if (avbsf_ctx_) { av_bsf_free(&avbsf_ctx_); }
    }

    void register_packet_callback(const AvPacketCallback &packet_cb) { packet_cb_ = packet_cb; }

    void init_filter_by_codec_name(const std::string &name, const std::shared_ptr<AVCodecParameters> &codecpar) {
        if (codecpar->extradata_size >= 4) {
            if (codecpar->extradata[0] == 0x00 && codecpar->extradata[1] == 0x00 && codecpar->extradata[2] == 0x00
                && codecpar->extradata[3] == 0x01) {
                char name[32];
                sprintf(name, "%.4s%s", avcodec_find_decoder(codecpar->codec_id)->name, "_mp4toannexb");
                av_bit_stream_filter_ = av_bsf_get_by_name(name);
                init_bsf_alloc(codecpar);
            } else if (codecpar->extradata[0] == 0x00 && codecpar->extradata[1] == 0x00
                       && codecpar->extradata[2] == 0x01) {
                char name[32];
                sprintf(name, "%.4s%s", avcodec_find_decoder(codecpar->codec_id)->name, "_mp4toannexb");
                av_bit_stream_filter_ = av_bsf_get_by_name(name);
                init_bsf_alloc(codecpar);
            }
        }
    }

    void send_packet(const std::shared_ptr<AVPacket> &packet) {
        if (av_bit_stream_filter_) {
            // 1. push packet to filter
            if (av_bsf_send_packet(avbsf_ctx_, packet.get()) < 0) {
                throw std::runtime_error("Error during decoding filter");
            }

            // 2. read packet from filter
            auto receive_packet =
                std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *ptr) { av_packet_free(&ptr); });
            while (av_bsf_receive_packet(avbsf_ctx_, receive_packet.get()) == 0) {
                // packet ready
                if (packet_cb_) { packet_cb_(receive_packet); }
                // unref the
                av_packet_unref(receive_packet.get());
            }
        } else {
            if (packet_cb_) { packet_cb_(packet); }
        }
    }

protected:
    void init_bsf_alloc(const std::shared_ptr<AVCodecParameters> &codecpar) {
        if (av_bsf_alloc(av_bit_stream_filter_, &avbsf_ctx_) < 0) {
            throw std::runtime_error("Failed to alloc av_bsf_alloc");
        }
        if (avcodec_parameters_copy(avbsf_ctx_->par_in, codecpar.get()) < 0) {
            throw std::runtime_error("Failed to alloc av bsf avcodec_parameters_copy");
        }
        if (av_bsf_init(avbsf_ctx_) < 0) { throw std::runtime_error("Failed to init bitstream filter"); }
        spdlog::info("Success to init BitStream Filter");
    }

protected:
    const AVBitStreamFilter *av_bit_stream_filter_{nullptr};
    AVBSFContext *avbsf_ctx_{nullptr};

    AvPacketCallback packet_cb_;
};

}// namespace av_wrapper

#endif// __BITSTRAM_FILTER_V3_HPP_