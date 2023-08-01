#include "draw_image.h"
#include "json.hpp"
#include "utils.hpp"

const int skeleton_17[][2] = {{15, 13}, {13, 11}, {16, 14}, {14, 12}, {11, 12}, {5, 11}, {6, 12},
                              {5, 6},   {5, 7},   {6, 8},   {7, 9},   {8, 10},  {1, 2},  {0, 1},
                              {0, 2},   {1, 3},   {2, 4},   {3, 5},   {4, 6}};
const int skeleton_5[][2] = {{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};

namespace gddi {

class BlendParallel : public cv::ParallelLoopBody {
public:
    BlendParallel(const cv::Mat &img1, const cv::Mat &img2, cv::Mat &blended_image, double alpha)
        : img1_(img1), img2_(img2), blended_image_(blended_image), alpha_(alpha) {}

    void operator()(const cv::Range &range) const {
        for (int y = range.start; y < range.end; ++y) {
            for (int x = 0; x < img1_.cols; ++x) {
                cv::Vec4b px1 = img1_.at<cv::Vec4b>(y, x);
                cv::Vec4b px2 = img2_.at<cv::Vec4b>(y, x);

                // Step 1: Pre-blend alpha channels
                double a1 = px1[3] * alpha_;
                double a2 = px2[3] * (1 - alpha_);
                double a_blended = a1 + a2;

                // Step 2: Calculate alpha-weighted color values
                double b1 = px1[0] * a1;
                double g1 = px1[1] * a1;
                double r1 = px1[2] * a1;
                double b2 = px2[0] * a2;
                double g2 = px2[1] * a2;
                double r2 = px2[2] * a2;

                // Step 3: Blend color values using pre-blended alpha channel
                int b = a_blended != 0 ? static_cast<int>((b1 + b2) / a_blended) : 0;
                int g = a_blended != 0 ? static_cast<int>((g1 + g2) / a_blended) : 0;
                int r = a_blended != 0 ? static_cast<int>((r1 + r2) / a_blended) : 0;

                blended_image_.at<cv::Vec3b>(y, x) = cv::Vec3b(b, g, r);
            }
        }
    }

private:
    const cv::Mat &img1_;
    const cv::Mat &img2_;
    cv::Mat &blended_image_;
    double alpha_;
};

cv::Mat weighted_blend2(const cv::Mat &img1_bgra, const cv::Mat &img2_bgra, double alpha) {
    // cv::Mat img1_bgra, img2_bgra;
    // cv::cvtColor(img1, img1_bgra, cv::COLOR_BGR2BGRA);
    // cv::cvtColor(img2, img2_bgra, cv::COLOR_BGR2BGRA);

    cv::Mat blended_image(img1_bgra.rows, img1_bgra.cols, CV_8UC3);

    // 创建并行融合操作的实例
    BlendParallel blend_parallel(img1_bgra, img2_bgra, blended_image, alpha);

    // 使用 parallel_for_ 函数执行并行处理
    parallel_for_(cv::Range(0, img1_bgra.rows), blend_parallel);

    return blended_image;
}

DrawImage::DrawImage() { graphics_ = std::make_unique<graphics::Graphics>(); }

DrawImage::~DrawImage() = default;

bool DrawImage::init_drawing(const std::string &font_face, const std::string &background) {
    graphics_->set_font_face(font_face);

    std::string json_data;
    std::vector<Block> blocks;
    if (utils::read_file(background, json_data)) {
        auto obj = nlohmann::json::parse(json_data);

        if (obj.count("imgfile") > 0) {
            blit_image_ = cv::imread("/home/resources/" + obj["imgfile"].get<std::string>(), cv::IMREAD_UNCHANGED);
        }

        for (const auto &[key, values] : obj.items()) {
            if (key == "constant") {
                graphics_->set_image(blit_image_);
                for (const auto &item : values) {
                    auto text = item["text"].get<std::string>();
                    auto coordinate = item["coordinate"].get<std::vector<int>>();
                    int font_size = item["font_size"].get<int>();
                    graphics_->draw_text(Point2i{coordinate[0], coordinate[1] + font_size}, text,
                                         Scalar{255, 255, 255, 1}, font_size);
                }
                blit_image_ = graphics_->get_image();
            } else if (key == "variable") {
                for (const auto &item : values) {
                    auto text = item["text"].get<std::string>();
                    if (text[0] == '$') {
                        blocks_[text.substr(1, text.size() - 1)] =
                            Block{item["coordinate"].get<std::vector<int>>(), item["font_size"].get<int>()};
                    }
                }
            }
        }

        encode_time_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }

    return true;
}

void DrawImage::draw_frame(const std::shared_ptr<nodes::FrameInfo> &frame_info) {
    auto &back_ext_info = frame_info->ext_info.back();

    if (++encode_count_ > frame_info->infer_frame_idx) {
        encode_count_ = 0;
        ++encode_time_;
    }

    cv::Mat cur_blit_image;
    if (!blit_image_.empty()) {
        graphics_->set_image(blit_image_.clone());
        std::map<std::string, int> label_count;
        for (const auto &item : back_ext_info.cross_count) {
            for (const auto &[key, value] : item) {
                label_count[key + "_0"] += value[0];
                label_count["total_0"] += value[0];
                label_count[key + "_1"] += value[1];
                label_count["total_1"] += value[1];
            }
        }

        for (const auto &[key, block] : blocks_) {
            if (key == "datetime") {
                std::tm *local_time = std::localtime(&encode_time_);
                std::ostringstream oss;
                oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
                graphics_->draw_text(Point2i{block.coordinate[0], block.coordinate[1] + block.font_size}, oss.str(),
                                     Scalar{255, 255, 255, 255}, block.font_size);
            } else if (label_count.count(key) > 0) {
                graphics_->draw_text(Point2i{block.coordinate[0], block.coordinate[1] + block.font_size},
                                     std::to_string(label_count[key]), Scalar{255, 255, 255, 255}, block.font_size);
            }
        }

        cur_blit_image = graphics_->get_image();
    }

    graphics_->set_image(image_wrapper::image_to_mat(frame_info->src_frame->data));

    // 越线计数可视化
    for (const auto &points : back_ext_info.border_points) {
        graphics_->draw_line(Point2i{points[0].x, points[0].y}, Point2i{points[1].x, points[1].y},
                             Scalar{255, 0, 0, 255 * 0.5}, 2);
    }

    for (auto &[key, points] : frame_info->roi_points) {
        graphics_->draw_polygon_fill(points, Scalar{116, 0, 0, 255 * 0.2});
    }

    if (back_ext_info.algo_type == AlgoType::kDetection || back_ext_info.algo_type == AlgoType::kClassification) {
        for (const auto &[_, item] : back_ext_info.tracked_box) {
            auto color = back_ext_info.map_class_color.at(item.class_id);
            graphics_->draw_rect(item.box, item.prob, back_ext_info.map_class_color[item.class_id], 2);
            graphics_->draw_text_fill(Point2i{(int)item.box.x - 3, (int)item.box.y - 20},
                                      back_ext_info.map_class_label.at(item.class_id) + " "
                                          + std::to_string(item.prob).substr(0, 4),
                                      back_ext_info.map_class_color.at(item.class_id));
        }
    } else if (back_ext_info.algo_type == AlgoType::kPose) {
        for (const auto &[_, points] : back_ext_info.map_key_points) {
            if (points.size() == 17) {
                for (const auto &bone : skeleton_17) {
                    Scalar color;
                    if (bone[0] < 5 || bone[1] < 5) color = {0, 255, 0, 255};
                    else if (bone[0] > 12 || bone[1] > 12)
                        color = {255, 0, 0, 255};
                    else if (bone[0] > 4 && bone[0] < 11 && bone[1] > 4 && bone[1] < 11)
                        color = {0, 255, 255, 255};
                    else
                        color = {255, 0, 255, 255};

                    graphics_->draw_line(Point2i{(int)points[bone[0]].x, (int)points[bone[0]].y},
                                         Point2i{(int)points[bone[1]].x, (int)points[bone[1]].y}, color, 2);
                }
            } else if (points.size() == 5) {
                for (const auto &bone : skeleton_5) {
                    graphics_->draw_line(Point2i{(int)points[bone[0]].x, (int)points[bone[0]].y},
                                         Point2i{(int)points[bone[1]].x, (int)points[bone[1]].y},
                                         Scalar({0, 255, 0, 255}), 2);
                }
            }

            for (auto &item : points) {
                Scalar color;
                if (item.number < 5) color = {0, 255, 0, 255};
                else if (item.number > 10)
                    color = {255, 0, 0, 255};
                else
                    color = {0, 255, 255, 255};
                graphics_->draw_circle(Point2i{(int)item.x, (int)item.y}, color, 3);
            }
        }
    } else if (back_ext_info.algo_type == AlgoType::kOCR_REC) {
        for (const auto &[_, item] : back_ext_info.map_ocr_info) {
            graphics_->draw_rect(Rect2f{item.points[0].x, item.points[0].y, item.points[2].x - item.points[0].x,
                                        item.points[2].y - item.points[0].y},
                                 1, Scalar{255, 0, 0, 255}, 2);

            std::string text;
            for (auto &value : item.labels) { text += value.str; }
            graphics_->draw_text_fill(Point2i{(int)item.points[0].x, (int)item.points[0].y}, text,
                                      Scalar{255, 0, 0, 255});
        }
    }

    auto tgt_frame = graphics_->get_image();
    if (!cur_blit_image.empty()) {
        cv::resize(cur_blit_image, cur_blit_image, cv::Size(frame_info->width(), frame_info->height()));
        tgt_frame = weighted_blend2(tgt_frame, cur_blit_image, 0.2);
    }

#ifdef WITH_BM1684
    cv::cvtColor(tgt_frame, frame_info->tgt_frame, cv::COLOR_BGRA2BGR);
#else
    frame_info->tgt_frame = tgt_frame;
#endif
}

}// namespace gddi