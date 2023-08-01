//
// Created by cc on 2021/11/09.
//

#include "encode_video_v3.h"
#include <memory>
#include <spdlog/spdlog.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}

namespace av_wrapper {

#define STEP_ALIGNMENT 32
#define BM_ALIGN16(_x) (((_x) + 0x0f) & ~0x0f)
#define BM_ALIGN32(_x) (((_x) + 0x1f) & ~0x1f)
#define BM_ALIGN64(_x) (((_x) + 0x3f) & ~0x3f)

static AVPixelFormat _convert_deprecated_format(enum AVPixelFormat format) {
    switch (format) {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P; break;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P; break;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P; break;
        case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P; break;
        default: return format; break;
    }
}

static AVPixelFormat find_decoder_hw_config(AVCodec *decoder, AVHWDeviceType type) {
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n", decoder->name,
                    av_hwdevice_get_type_name(type));
            return AV_PIX_FMT_NONE;
        }

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
            return config->pix_fmt;
        }
    }
}

class EncoderPrivate {
public:
    EncoderPrivate(Encoder_v3::OpenCallback open_cb, Encoder_v3::PacketCallback packet_cb)
        : open_cb_(std::move(open_cb)), packet_cb_(std::move(packet_cb)) {}
    ~EncoderPrivate() {}

    bool open_encoder(const EncodeOptions &opts, const AVHWDeviceType type);
    bool encode_frame(const std::shared_ptr<AVFrame> &frame);
    bool encode_frame(const uint8_t *data, int width, int height);
    void flush_encoder();

private:
    const AVCodec *encoder = nullptr;
    std::unique_ptr<AVCodecContext, void (*)(AVCodecContext *)> codec_ctx_{nullptr, nullptr};

    AVBufferRef *hw_device_ctx = nullptr;
    AVHWFramesContext *frames_ctx = nullptr;
    enum AVPixelFormat hw_pix_fmt;

    uint64_t frame_index = 0;
    std::shared_ptr<AVFrame> last_frame_;

    Encoder_v3::OpenCallback open_cb_;
    Encoder_v3::PacketCallback packet_cb_;
};

bool EncoderPrivate::open_encoder(const EncodeOptions &opts, const AVHWDeviceType type) {
    try {
        encoder = avcodec_find_encoder_by_name(opts.codec.c_str());

        if (!encoder) { throw std::runtime_error("Necessary encoder not found: " + opts.codec); }

        codec_ctx_ = std::unique_ptr<AVCodecContext, void (*)(AVCodecContext *)>(
            avcodec_alloc_context3(encoder), [](AVCodecContext *ptr) { avcodec_free_context(&ptr); });

        if (!codec_ctx_.get()) { throw std::runtime_error("Failed to allocate the encoder context"); }

        codec_ctx_->codec_id = encoder->id;
        codec_ctx_->width = opts.width;
        codec_ctx_->height = opts.height;
        codec_ctx_->pix_fmt = opts.pix_fmt;
        codec_ctx_->bit_rate_tolerance = opts.bit_rate_tolerance;
        codec_ctx_->bit_rate = opts.bit_rate;
        codec_ctx_->time_base = {1, opts.framerate};
        codec_ctx_->framerate = {opts.framerate, 1};
        // codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER | AV_CODEC_FLAG_LOW_DELAY;
        codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
        codec_ctx_->gop_size = opts.gop_size;
        codec_ctx_->profile = FF_PROFILE_H264_MAIN;

        if (type != AV_HWDEVICE_TYPE_NONE) {
            if (av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0) < 0) {
                throw std::runtime_error("Failed to create specified HW device");
            }

            if (!(codec_ctx_->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx))) {
                throw std::runtime_error("Failed to create cuda frame context");
            }

            frames_ctx = (AVHWFramesContext *)(codec_ctx_->hw_frames_ctx->data);
            frames_ctx->format = opts.pix_fmt;
            frames_ctx->sw_format = AV_PIX_FMT_NV12;
            frames_ctx->width = opts.width;
            frames_ctx->height = opts.height;
            frames_ctx->initial_pool_size = 20;

            if (av_hwframe_ctx_init(codec_ctx_->hw_frames_ctx) < 0) {
                av_buffer_unref(&codec_ctx_->hw_frames_ctx);
                throw std::runtime_error("Failed to initialize CUDA frame context");
            }
        }

        AVDictionary *dict = NULL;
#if defined(WITH_BM1684)
        av_dict_set_int(&dict, "sophon_idx", 0, 0);
        av_dict_set_int(&dict, "gop_preset", 8, 0);
        /* Use system memory */
        av_dict_set_int(&dict, "is_dma_buffer", 0, 0);
        av_dict_set_int(&dict, "roi_enable", 0, 0);

        last_frame_ = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *ptr) { av_frame_free(&ptr); });
        last_frame_->format = opts.pix_fmt;
        last_frame_->width = opts.width;
        last_frame_->height = opts.height;
#endif

        if (avcodec_open2(codec_ctx_.get(), encoder, &dict) < 0) {
            throw std::runtime_error("Cannot open video encoder codec");
        }

        if (open_cb_) {
            auto codec_parameters = std::shared_ptr<AVCodecParameters>(
                avcodec_parameters_alloc(), [](AVCodecParameters *ptr) { avcodec_parameters_free(&ptr); });
            avcodec_parameters_from_context(codec_parameters.get(), codec_ctx_.get());
            open_cb_(codec_parameters, codec_ctx_->time_base, codec_ctx_->framerate);
        }
    } catch (std::exception &e) {
        spdlog::error(e.what());
        return false;
    }

    return true;
}

bool EncoderPrivate::encode_frame(const std::shared_ptr<AVFrame> &frame) {
    try {
        auto enc_frame = frame;

        std::shared_ptr<uint8_t> buffer;
        if (enc_frame->width != codec_ctx_->width || enc_frame->height != codec_ctx_->height) {
            enc_frame = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *ptr) { av_frame_free(&ptr); });

            enc_frame->width = codec_ctx_->width;
            enc_frame->height = codec_ctx_->height;
            enc_frame->format = _convert_deprecated_format((AVPixelFormat)enc_frame->format);
            buffer = std::shared_ptr<uint8_t>(
                (uint8_t *)av_malloc(av_image_get_buffer_size((AVPixelFormat)enc_frame->format, codec_ctx_->width,
                                                              codec_ctx_->height, 1)),
                [](uint8_t *buf) { av_free(buf); });
            av_image_fill_arrays(enc_frame->data, enc_frame->linesize, buffer.get(), (AVPixelFormat)enc_frame->format,
                                 enc_frame->width, enc_frame->height, 1);

            SwsContext *conversion = sws_getContext(
                enc_frame->width, enc_frame->height, (AVPixelFormat)enc_frame->format, enc_frame->width,
                enc_frame->height, (AVPixelFormat)enc_frame->format, SWS_FAST_BILINEAR, NULL, NULL, NULL);
            sws_scale(conversion, enc_frame->data, enc_frame->linesize, 0, enc_frame->height, enc_frame->data,
                      enc_frame->linesize);
            sws_freeContext(conversion);
        }

        if (hw_device_ctx) {
            enc_frame = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *ptr) { av_frame_free(&ptr); });
            if (!enc_frame.get()) { throw std::runtime_error("Can not alloc frame"); }

            if (av_hwframe_get_buffer(codec_ctx_->hw_frames_ctx, enc_frame.get(), 0) < 0) {
                throw std::runtime_error("Failed to convert hwframe");
            }

            if (!enc_frame->hw_frames_ctx) { throw std::runtime_error("Frame is not hwaccel pixel format"); }

            if (av_hwframe_transfer_data(enc_frame.get(), frame.get(), 0) < 0) {
                throw std::runtime_error("Error while transferring frame data to surface");
            }

            av_frame_copy_props(enc_frame.get(), frame.get());

            enc_frame->pts = frame_index++;
            if (avcodec_send_frame(codec_ctx_.get(), enc_frame.get()) < 0) {
                throw std::runtime_error("Error sending a frame for encoding");
            }
        } else {
            enc_frame->pts = frame_index++;
            if (avcodec_send_frame(codec_ctx_.get(), frame.get()) < 0) {
                throw std::runtime_error("Error sending a frame for encoding");
            }
        }

        last_frame_ = enc_frame;

        while (true) {
            auto packet = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *pkt) { av_packet_free(&pkt); });
            int ret = avcodec_receive_packet(codec_ctx_.get(), packet.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
#if !defined(WITH_MLU220)
            // TODO: MLU220 暂时忽略这个报错
            else if (ret < 0)
                throw std::runtime_error("Error during encoding");
#endif
            packet->stream_index = 0;
            if (packet_cb_) packet_cb_(packet);
        }
    } catch (const std::exception &e) {
        spdlog::error(e.what());
        return false;
    }

    return true;
}

bool EncoderPrivate::encode_frame(const uint8_t *data, int width, int height) {
    int step = (width + STEP_ALIGNMENT - 1) & ~(STEP_ALIGNMENT - 1);

    try {
        if (step % STEP_ALIGNMENT != 0) { throw std::runtime_error("input step must align with STEP_ALIGNMENT"); }

        av_image_fill_arrays(last_frame_->data, last_frame_->linesize, (uint8_t *)data, codec_ctx_->pix_fmt, width,
                             height, 1);
        last_frame_->linesize[0] = step;

        last_frame_->pts = frame_index;
        frame_index++;

        if (avcodec_send_frame(codec_ctx_.get(), last_frame_.get()) < 0) {
            throw std::runtime_error("Error sending a frame for encoding");
        }

        while (true) {
            auto packet = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *pkt) { av_packet_free(&pkt); });
            if (avcodec_receive_packet(codec_ctx_.get(), packet.get()) < 0) break;

            if (packet_cb_) packet_cb_(packet);
        }

    } catch (const std::exception &e) {
        spdlog::error(e.what());
        return false;
    }

    return true;
}

void EncoderPrivate::flush_encoder() {
    if (!(codec_ctx_->codec->capabilities & AV_CODEC_CAP_DELAY)) return;

    try {
        if (avcodec_send_frame(codec_ctx_.get(), last_frame_.get()) < 0) {
            spdlog::error("Error sending a frame for encoding");
        }

        while (true) {
            auto packet = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *pkt) { av_packet_free(&pkt); });
            if (avcodec_receive_packet(codec_ctx_.get(), packet.get()) < 0) break;
            if (packet_cb_) packet_cb_(packet);
        }
    } catch (const std::exception &e) { spdlog::error(e.what()); }
}

Encoder_v3::Encoder_v3() {}

Encoder_v3::~Encoder_v3() {}

bool Encoder_v3::open_encoder(const EncodeOptions &opts, const AVHWDeviceType type) {
    impl_ = std::make_unique<EncoderPrivate>(open_cb_, packet_cb_);
    return impl_->open_encoder(opts, type);
}

void Encoder_v3::close_encoder() {
    if (impl_) impl_->flush_encoder();
    impl_.reset();
}

bool Encoder_v3::encode_frame(const std::shared_ptr<AVFrame> &frame) { return impl_->encode_frame(frame); }

bool Encoder_v3::encode_frame(const uint8_t *data, int width, int height) {
    return impl_->encode_frame(data, width, height);
}

void Encoder_v3::register_open_callback(const OpenCallback &open_cb) { open_cb_ = open_cb; }
void Encoder_v3::register_packet_callback(const PacketCallback &packet_cb) { packet_cb_ = packet_cb; }

}// namespace av_wrapper