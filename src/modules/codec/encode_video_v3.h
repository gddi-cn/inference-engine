#ifndef __ENCODER_VIDEO_V3_H__
#define __ENCODER_VIDEO_V3_H__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}
#include <functional>
#include <memory>
#include <string>

namespace av_wrapper {

struct EncodeOptions {
    int width = 1920;
    int height = 1080;
    int framerate = 25;

#if defined(WITH_BM1684)
    std::string codec = "h264_bm";
    AVPixelFormat pix_fmt{AV_PIX_FMT_YUV420P};
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
    std::string codec = "h264_mluenc";
    AVPixelFormat pix_fmt{AV_PIX_FMT_MLU};
#else
    std::string codec = "h264_nvenc";
    AVPixelFormat pix_fmt{AV_PIX_FMT_CUDA};
#endif

    int bit_rate = 4000000;
    int bit_rate_tolerance = 200000;
    int gop_size = 32;
};

class EncoderPrivate;

class Encoder_v3 {
public:
    /**
     * @brief 编码器回调
     * 
     */
    using OpenCallback =
        std::function<void(const std::shared_ptr<AVCodecParameters> &, const AVRational &, const AVRational &)>;
    /**
     * @brief 编码帧回调
     * 
     */
    using PacketCallback = std::function<void(const std::shared_ptr<AVPacket> &)>;

public:
    Encoder_v3();
    ~Encoder_v3();

    /**
     * @brief 初始化编码器
     * 
     * @param opts 
     * @param type 
     * @return true 
     * @return false 
     */
    bool open_encoder(const EncodeOptions &opts, const AVHWDeviceType type);

    /**
     * @brief 关闭编码器
     */
    void close_encoder();

    /**
     * @brief 编码一帧
     * 
     * @param frame 
     * @return true 
     * @return false 
     */
    bool encode_frame(const std::shared_ptr<AVFrame> &frame);

    /**
     * @brief 编码一帧
     * 
     * @param frame 
     * @return true 
     * @return false 
     */
    bool encode_frame(const uint8_t *data, int width, int height);

    /**
     * @brief 注册编码器回调
     * 
     * @param open_cb 
     */
    void register_open_callback(const OpenCallback &open_cb);

    /**
     * @brief 注册编码帧回调
     * 
     * @param open_cb 
     */
    void register_packet_callback(const PacketCallback &packet_cb);

private:
    std::unique_ptr<EncoderPrivate> impl_;

    OpenCallback open_cb_;
    PacketCallback packet_cb_;
};

}// namespace av_wrapper

#endif// __ENCODER_VIDEO_V3_H__
