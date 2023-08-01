#ifndef __ABSTRACT_ALG_H__
#define __ABSTRACT_ALG_H__

#include "basic_logs.hpp"
#include "json.hpp"
#include "nodes/node_struct_def.h"
#include <memory>
#include <string>
#include <vector>

namespace gddi {
namespace algo {

enum class InferType { kAsync, kSync };

struct ModParms {
    int stream_id;// 对应 bitmain 推理的 m_streamId

    std::string mod_id;
    std::string mod_name;
    std::string mod_path;
    std::vector<std::string> lib_paths;
    float mod_thres{0};
    float frame_rate{15};
    size_t instances{1};
    uint32_t batch_size{1};
};

struct AlgoOutput {
    int class_id;                                                    // 类别 ID
    std::string label;                                               // 类别标签
    float prob;                                                      // 目标置信度
    Rect2f box;                                                      // 目标框
    std::vector<std::tuple<int, float, float, float>> vec_key_points;// 关键点坐标 number, x, y, prob

    int seg_width;                                 // 分割图宽高
    int seg_height;                                // 分割图宽高
    std::vector<uint8_t> seg_map;                  // 分割图
    std::vector<float> feature;                    // 特征图
    std::string ocr_str;                           // 车牌OCR
    std::vector<gddi::nodes::OcrInfo> vec_ocr_info;// 通用OCR
};

using InitCallback = std::function<void(const std::vector<std::string> &)>;
using InferCallback = std::function<void(const int64_t, const AlgoType, const std::vector<algo::AlgoOutput> &)>;

class AbstractAlgo {
public:
    AbstractAlgo() {}
    virtual ~AbstractAlgo() {}

    virtual bool init(const ModParms &parms) { return init(parms, AlgoType::kUndefined, std::vector<std::string>()); }

    virtual void inference(const std::string &task_name, const std::shared_ptr<nodes::FrameInfo> &info,
                           const InferType type) = 0;

    virtual AlgoType inference(const std::string &task_name, const std::shared_ptr<nodes::FrameInfo> &info,
                               std::map<int, std::vector<algo::AlgoOutput>> &outputs) = 0;

    void register_init_callback(const InitCallback &callback) { init_callback_ = callback; }
    void register_infer_callback(const InferCallback &callback) { infer_callback_ = callback; }

protected:
    virtual bool init(const ModParms &parms, const AlgoType type, const std::vector<std::string> &vec_labels) {
        if (init_callback_) { init_callback_(vec_labels); }

        spdlog::info("############################## MODEL "
                     "##############################");
        spdlog::info("Model Name: {}", parms.mod_name);
        spdlog::info("Model Path: {}", parms.mod_path);

        char buffer[512000] = {0};
        sprintf(buffer, "Model Label: [ ");
        for (auto &item : vec_labels) { sprintf(buffer + strlen(buffer), "\"%s\" ", item.c_str()); }
        spdlog::info("{} {}", buffer, "]");
        spdlog::info("Model Threshold: {}", parms.mod_thres);
        spdlog::info("Model Type: {}", (int)type);
        spdlog::info("###################################################################");

        return true;
    };

    std::vector<std::vector<nodes::PoseKeyPoint>>
    complementary_frame(const std::vector<std::vector<nodes::PoseKeyPoint>> &key_points, const size_t clip_len = 20) {
        std::vector<std::vector<nodes::PoseKeyPoint>> out_key_points;

        if (key_points.size() < clip_len) {
            out_key_points = key_points;
            // 补帧
            do {
                out_key_points.insert(out_key_points.end(), key_points.begin(), key_points.end());
            } while (out_key_points.size() < clip_len);
            out_key_points.resize(clip_len);
        } else if (key_points.size() > clip_len) {
            // 抽帧
            float bsize = key_points.size() * 1.0 / clip_len;
            for (float i = 0; i < clip_len; i++) {
                // 随机数
                int rand_num = std::rand() / ((RAND_MAX + 1u) / int(bsize));
                out_key_points.emplace_back(key_points[int(i * bsize + 0.5) + rand_num]);
            }
        } else {
            out_key_points = key_points;
        }

        assert(out_key_points.size() == clip_len);

        return out_key_points;
    }

protected:
    InitCallback init_callback_;
    InferCallback infer_callback_;
};
}// namespace algo
}// namespace gddi
#endif