#ifndef __DECODER_V3_H__
#define __DECODER_V3_H__

extern "C" {
#include <libavcodec/avcodec.h>
}
#include <memory>
#include <functional>

namespace av_wrapper {

class DecoderPrivate;

class Decoder_v3 {
public:
    using OpenCallback = std::function<void(const std::shared_ptr<AVCodecParameters> &)>;
    using DecodeCallback = std::function<bool(const int64_t, const std::shared_ptr<AVFrame> &)>;

public:
    Decoder_v3();
    ~Decoder_v3();

    /**
         * @brief 打开解码器，通过指定的 AVCodecParameters 参数
         * 
         * @param codecpar 
         * @param type 硬件加速器类型
         * @return true 
         * @return false 
         */
    bool open_decoder(const std::shared_ptr<AVCodecParameters> &codecpar,
                      const AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE);

    /**
     * @brief 解码压缩帧
     * 
     * @param packet 
     * @return bool 
     */
    bool decode_packet(const std::shared_ptr<AVPacket> &packet);

    /**
     * @brief 注册解码器打开回调
     * 
     * @param open_cb 
     */
    void register_open_callback(const OpenCallback &open_cb);

    /**
     * @brief 注册解码回调
     * 
     * @param decode_cb 
     */
    void register_deocde_callback(const DecodeCallback &decode_cb);

private:
    std::unique_ptr<DecoderPrivate> impl_;

    OpenCallback open_cb_;
    DecodeCallback decode_cb_;
};
}// namespace av_wrapper

#endif