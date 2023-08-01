#pragma once

#if defined(WITH_RV1126)

#include <rga/rga.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/rk_venc_cfg.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace av_wrapper {

enum class CodingType { kH264, kHEVC };

using CodecCallback = std::function<void(const int64_t image_id, const uint8_t *yuv, const uint32_t width,
                                         const uint32_t height, const RgaSURF_FORMAT format)>;

class MppDecoder {
public:
    MppDecoder();
    ~MppDecoder();

    bool open_decoder(CodingType type, const int width, const int height);
    bool decode(const uint8_t *data, const uint32_t size, const CodecCallback &callback);
    void close_decoder();

private:
    MppCtx ctx = nullptr;
    MppApi *mpi = nullptr;

    char *pkt_buf = nullptr;
    MppPacket packet = nullptr;

    MppFrame frame = nullptr;

    uint32_t image_id = 0;
};

}// namespace av_wrapper

#endif