#ifndef __NODE_STRUCT_DEF_H__
#define __NODE_STRUCT_DEF_H__

#include "modules/mem_pool.hpp"
#include "modules/types.hpp"
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <json.hpp>

#if defined(WITH_OPENCV)
#include <opencv2/core.hpp>
#endif

#ifdef WITH_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
}
#endif

#include "modules/wrapper/tsing_wrapper.hpp"

namespace gddi {

enum class AlgoType {
    kUndefined = 0,
    kDetection,
    kClassification,
    kPose,
    kSegmentation,
    kAction,
    kOCR,
    kFace,
    kOCR_DET,
    kOCR_REC
};
enum class ActionType { kUndefined = 0, kBase, kCount };
enum class TaskType { kUndefined = 0, kCamera, kVideo, kImage, kAsyncImage };

// None: 空帧; Base: 普通帧; Report: 上报帧; Disappear: 目标消失帧
enum class FrameType { kNone = 0, kBase, kReport, kDisappear };

namespace nodes {
enum class NODE_EXIT_CODE { NORMAL = 0, ABNORMAL, FINISH, PAUSE };

struct TrackInfo {
    int target_id;// 目标 ID
    int class_id; // 类别 ID
    float prob;   // 目标阈值
    Rect2f box;   // 跟踪预测框 x, y, width, height
};

struct BoxInfo {
    int prev_id;       // 上层 ID
    int class_id;      // 类别 ID
    float prob;        // 目标阈值
    Rect2f box;        // 目标框 x, y, width, height
    std::string roi_id;// 所属 ROI 标签
    int track_id;      // 跟踪 ID
    int distance;      // 告警距离
    bool moving{false};// 是否移动
};

struct PoseKeyPoint {
    int number;
    float x;
    float y;
    float prob;
};

struct LableInfo {
    int class_id;   // 类别 ID
    float prob;     // 目标阈值
    std::string str;// 类别名
};

struct OcrInfo {
    std::string roi_id;// 所属 ROI 标签
    std::vector<PoseKeyPoint> points;
    std::vector<LableInfo> labels;
};

struct SegContour {
    float x;          // 轮廓中心点 - x
    float y;          // 轮廓中心点 - y
    double area{-1};  // 分割面积
    double length{-1};// 分割长度
    double volume{-1};// 分割体积
};

struct FeatureInfo_v1 {
    char path[256];
    uint8_t sha256[32];
    float feature[512];
};

struct FrameExtInfo {
    explicit FrameExtInfo(const AlgoType type, std::string id, std::string name, const float thres)
        : algo_type(type), mod_id(std::move(id)), mod_name(std::move(name)), mod_thres(thres) {}

    AlgoType algo_type;  // 算法(模型)类型
    std::string mod_id;  // 模型ID
    std::string mod_name;// 模型名称
    float mod_thres;     // 模型阈值

    bool flag_crop{false};// 多阶段裁剪标志

    std::map<int, cv::Mat> crop_images;// 扣图图像

    std::map<int, Rect2f> crop_rects;// 扣图座标信息

    std::map<int, std::string> map_class_label;// 标签映射
    std::map<int, Scalar> map_class_color;     // 颜色映射

    // 存储原始推理结果
    std::map<int, BoxInfo> infer_target_info;

    // 目标框信息 -- (目标编号 - 框标信息)
    std::map<int, BoxInfo> map_target_box;

    int seg_width;                                          // 掩码图宽
    int seg_height;                                         // 掩码图高
    std::vector<uint8_t> seg_map;                           // 分割掩码图
    std::map<uint8_t, std::vector<SegContour>> seg_contours;// 分割轮廓信息

    // 关键点信息
    std::map<int, std::vector<PoseKeyPoint>> map_key_points;

    // 动作帧区间 -- (跟踪ID - 帧ID - 关键点集合)
    std::map<int, std::vector<std::vector<nodes::PoseKeyPoint>>> action_key_points;

    ActionType action_type{ActionType::kUndefined};// 动作类别
    // 动作分数 -- (跟踪ID - 动作类型 - 分数)
    std::map<int, std::pair<int, float>> cur_action_score;// 当前动作分数
    // 动作分数 -- (跟踪ID - 动作类型 - 分数集合)
    std::map<int, std::map<int, std::vector<float>>> sum_action_scores;// 累计动作分数

    // 跟踪信息 -- (跟踪ID - 座标信息)
    std::map<int, TrackInfo> tracked_box;// 跟踪框信息

    std::vector<Rect2f> mosaic_rects;// 马赛克信息

    // 特征矩阵 -- (跟踪ID - 矩阵)
    std::map<int, std::vector<float>> features;// 人脸识别特征

    std::vector<std::vector<Point2i>> border_points;                   // 边界点集合 (x1, y1) ... (xn, yn)
    std::vector<std::map<std::string, std::array<int, 2>>> cross_count;// 越界计数

    std::map<std::string, uint32_t> target_counts;// 目标计数

    // OCR 区域信息 -- (跟踪ID - 座标信息)
    std::map<int, OcrInfo> map_ocr_info;

    nlohmann::json metadata;// 元数据，特定业务场景存放特定信息
};

// 推理统一的返回结果，支持多 banch
struct FrameInfo {
    explicit FrameInfo(int64_t const idx, std::shared_ptr<gddi::MemObject<AVFrame>> const &src)
        : video_frame_idx(idx), infer_frame_idx(idx), src_frame(src) {
        timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
    }

    std::shared_ptr<gddi::MemObject<AVFrame>> src_frame;// 帧图像
    int width() const { return src_frame->data->width; }
    int height() const { return src_frame->data->height; }
    uint64_t area() const { return src_frame->data->width * src_frame->data->height; }

    FrameInfo(const FrameInfo &) = delete;
    FrameInfo &operator=(const FrameInfo &) = delete;

    explicit FrameInfo(std::shared_ptr<FrameInfo> const &other) {
        this->video_frame_idx = other->video_frame_idx;
        this->infer_frame_idx = other->infer_frame_idx;
        this->timestamp = other->timestamp;
        this->src_frame = other->src_frame;
        this->tgt_frame = other->tgt_frame;
        this->ext_info = other->ext_info;
        this->roi_points = other->roi_points;
    }

    int64_t video_frame_idx;           // 解码帧帧 ID
    int64_t infer_frame_idx;           // 推理帧 ID
    int64_t timestamp;                 // 帧时间戳
    std::vector<FrameExtInfo> ext_info;// 支持多阶段推理
    cv::Mat tgt_frame;                 // 已绘制帧图像

    int frame_event_result{-1};// 帧事件结果

    std::map<std::string, std::vector<Point2i>> roi_points;// ROI 信息
};

}// namespace nodes
}// namespace gddi

#endif