#include "opencv2/core/types.hpp"
#include <any>
#include <cstdint>
#include <functional>
#include <vector>

enum class PixelFormat { kNV12, kYUV420P, kBGR888 };

struct ImageInfo {
    int64_t image_id;
    uint32_t width;
    uint32_t height;
    PixelFormat format;

    ImageInfo &resize();
    std::vector<ImageInfo> crop(const std::vector<cv::Rect> &rects);
    std::vector<ImageInfo> crop(const std::vector<std::vector<cv::Point>> &polygons);
};

using ImageCallback = std::function<void(const ImageInfo &)>;

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool open_decoder(const std::string &url, ImageCallback callback);
    void close_decoder();
};