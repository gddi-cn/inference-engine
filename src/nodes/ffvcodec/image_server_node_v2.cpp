#include "image_server_node_v2.h"
#include "base64.h"
#include "common_basic/thread_dbg_utils.hpp"
#include "modules/network/downloader.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <memory>
#include <opencv2/core/base.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define ALIGN_WIDTH 16
#define ALGIN_HEIGHT 16
#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

namespace gddi {
namespace nodes {

const int skeleton[][2] = {{15, 13}, {13, 11}, {16, 14}, {14, 12}, {11, 12}, {5, 11}, {6, 12}, {5, 6}, {5, 7}, {6, 8},
                           {7, 9},   {8, 10},  {1, 2},   {0, 1},   {0, 2},   {1, 3},  {2, 4},  {3, 5}, {4, 6}};
const int skeleton_5[][2] = {{0, 1}, {0, 2}, {1, 2}, {0, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}};

ImageServer_v2::~ImageServer_v2() { http_server_stop(&server_); }

static ImageFormat get_image_format(const unsigned char *buffer, const size_t buffer_size) {
    if (((*buffer & 0x00FF) == 0xFF) && (*(buffer + 1) & 0xFF) == 0xD8) {
        return ImageFormat::kJPEG;
    } else if (((*buffer & 0x00FF) == 0x89) && (*(buffer + 1) & 0xFF) == 0x50) {
        return ImageFormat::kPNG;
    } else if (buffer_size == 1920 * 1080 * 3 / 2) {
        return ImageFormat::kNV12;
    } else if (buffer_size == 1920 * 1080 * 3) {
        return ImageFormat::kBGR;
    }
    return ImageFormat::kNone;
}

static Point2i find_centroid(const std::vector<Point2i> &points) {
    Point2i centroid = {0, 0};
    int n = points.size();

    for (const auto &p : points) {
        centroid.x += p.x;
        centroid.y += p.y;
    }

    centroid.x /= n;
    centroid.y /= n;

    return centroid;
}

float distance(const Point2i &p1, const Point2i &p2) { return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2)); }

float triangleArea(float a, float b, float c) {
    float s = (a + b + c) / 2;
    return sqrt(s * (s - a) * (s - b) * (s - c));
}

float circumradius(float a, float b, float c) {
    float area = triangleArea(a, b, c);
    return (a * b * c) / (4 * area);
}

float polygon_circumradius(const Point2i &centroid, const std::vector<Point2i> &points) {
    int n = points.size();
    float max_radius = std::numeric_limits<float>::min();

    for (int i = 0; i < n; ++i) {
        auto p1 = points[i];
        auto p2 = points[(i + 1) % n];
        auto p3 = centroid;

        float a = distance(p1, p2);
        float b = distance(p1, p3);
        float c = distance(p2, p3);

        float radius = circumradius(a, b, c);
        max_radius = std::max(max_radius, radius);
    }

    return max_radius;
}

static Rect2f algin_rect(const Rect2f &rect, int img_w, int img_h, float factor) {
    Rect2f resize_rect;

    // 目标框放大
    resize_rect.width = ALIGN(int(rect.width + rect.width * (factor - 1)), ALIGN_WIDTH);
    resize_rect.height = ALIGN(int(rect.height + rect.height * (factor - 1)), ALGIN_HEIGHT);
    if (resize_rect.width > img_w) resize_rect.width = img_w;
    if (resize_rect.height > img_h) resize_rect.height = img_h;

    // 重置顶点座标
    resize_rect.x = rect.x - (resize_rect.width - rect.width) / 2;
    resize_rect.y = rect.y - (resize_rect.height - rect.height) / 2;

    // 边界判断
    if (resize_rect.x < 0) resize_rect.x = 0;
    if (resize_rect.y < 0) resize_rect.y = 0;
    if (resize_rect.x + resize_rect.width > img_w) resize_rect.x = img_w - resize_rect.width;
    if (resize_rect.y + resize_rect.height > img_h) resize_rect.y = img_h - resize_rect.height;

    return resize_rect;
}

void ImageServer_v2::on_setup() {
    size_t concurrency = std::thread::hardware_concurrency();
    downloader_ = std::make_unique<network::Downloader>();

#if defined(WITH_NVIDIA)
    jpeg_decoder_ = std::make_unique<wrapper::NvJpegDecoder>(concurrency / 2, 1);
    mem_pool_.register_free_callback([](cv::cuda::GpuMat *image) { cudaFree(image->data); });
#endif

    router_.POST("/api/predict", [=](const HttpContextPtr &ctx) {
        on_request(ctx);
        return 0;
    });

    // srv_port_ = 9092; // DEBUG by cc
    server_.service = &router_;
    server_.port = srv_port_;
    server_.worker_threads = concurrency / 2;
    auto old_thread_name = gddi::thread_utils::get_cur_thread_name();
    gddi::thread_utils::set_cur_thread_name("img-server");
    http_server_run(&server_, 0);
    gddi::thread_utils::set_cur_thread_name(old_thread_name);
    spdlog::info("Image server task: {}, listen: {}", task_name_, srv_port_);
}

void ImageServer_v2::on_request(const HttpContextPtr &ctx) {
    try {
        auto req_obj = nlohmann::json::parse(ctx->body());

        auto raw_data = req_obj["image"].get<std::string>();
        nlohmann::json additional;
        if (req_obj.count("additional") > 0) { additional = req_obj["additional"]; }

        std::vector<uchar> img_data;
        if (raw_data.substr(0, 4) == "http") {
            downloader_->async_http_get(
                raw_data, [ctx, additional, this](const bool success, const std::vector<unsigned char> &img_data) {
                    if (success) {
                        on_construct(img_data, additional, ctx);
                    } else {
                        auto ack_json = nlohmann::json::object();
                        ack_json["success"] = false;
                        ack_json["error"] = std::string(img_data.begin(), img_data.end());
                        ctx->send(ack_json.dump());
                    }
                });
        } else {
            auto ptr = raw_data.data();
            int data_size = raw_data.size();
            if (raw_data.substr(0, 4) == "data") {
                auto pos = raw_data.find_first_of(',');
                if (pos == std::string::npos) { throw std::runtime_error("unsupport base64 format"); }
                ptr = raw_data.data() + pos + 1;
                data_size = raw_data.size() - pos - 1;
            }
            on_construct(Base64::decode(ptr, data_size, 0), additional, ctx);
        }

    } catch (std::exception &exception) {
        auto ack_json = nlohmann::json::object();
        ack_json["success"] = false;
        ack_json["error"] = exception.what();
        ctx->send(ack_json.dump());
    }
}

void ImageServer_v2::on_construct(const std::vector<uchar> &img_data, const nlohmann::json &additional,
                                  const HttpContextPtr &ctx) {
    try {
        auto format = get_image_format(img_data.data(), img_data.size());

        if (format == ImageFormat::kJPEG) {
            auto mem_obj = mem_pool_.alloc_mem_detach(0, 0);
            mem_obj->data = image_wrapper::image_jpeg_dec(img_data.data(), img_data.size());

            auto uuid = boost::uuids::to_string(boost::uuids::random_generator()());
            auto frame = std::make_shared<msgs::cv_frame>(uuid, TaskType::kAsyncImage, 1);
            frame->frame_info = std::make_shared<FrameInfo>(++frame_idx_, mem_obj);
            frame->response_callback_ = [ctx](const nlohmann::json &data, const bool code) {
                auto ack_json = nlohmann::json::object();
                ack_json["success"] = true;
                ack_json["data"] = data;
                ctx->send(ack_json.dump());
            };

            std::map<std::string, std::vector<std::vector<float>>> regions;
            if (additional.count("regions_with_label") > 0) {
                regions =
                    additional["regions_with_label"].get<std::map<std::string, std::vector<std::vector<float>>>>();
            }

            if (!regions.empty()) {
                // 构造一阶段检测的 ext_info
                nodes::FrameExtInfo ext_info(AlgoType::kDetection, "", "", 0);

                int index = 0;
                std::vector<Point2i> points;
                for (const auto &[key, values] : regions) {
                    std::vector<Point2i> points;
                    for (auto point : values) {
                        // 兼容比例和坐标
                        if (point[0] < 1) { point[0] = point[0] * frame->frame_info->width(); }
                        if (point[1] < 1) { point[1] = point[1] * frame->frame_info->height(); }
                        points.emplace_back(point[0], point[1]);
                    }
                    frame->frame_info->roi_points[key] = points;

                    auto x_compare = [](const Point2i &first, const Point2i &second) { return first.x < second.x; };
                    auto y_compare = [](const Point2i &first, const Point2i &second) { return first.y < second.y; };
                    float min_x = (*std::min_element(points.begin(), points.end(), x_compare)).x;
                    float min_y = (*std::min_element(points.begin(), points.end(), y_compare)).y;
                    float max_x = (*std::max_element(points.begin(), points.end(), x_compare)).x;
                    float max_y = (*std::max_element(points.begin(), points.end(), y_compare)).y;
                    float width = max_x - min_x;
                    float height = max_y - min_y;

                    if (width > height) {
                        min_y = min_y - (width - height) / 2;
                        height = width;
                    } else {
                        min_x = min_x - (height - width) / 2;
                        width = height;
                    }

                    ext_info.map_target_box.insert(std::make_pair(
                        index,
                        BoxInfo{.prev_id = 0,
                                .class_id = 0,
                                .prob = 1,
                                .box = {0, 0, (float)frame->frame_info->width(), (float)frame->frame_info->height()},
                                .roi_id = key,
                                .track_id = 0}));

                    ext_info.crop_rects[index] = algin_rect({min_x, min_y, width, height}, frame->frame_info->width(),
                                                            frame->frame_info->height(), scale_factor_);

                    ++index;
                }

                ext_info.flag_crop = true;
                ext_info.crop_images = image_wrapper::image_crop(mem_obj->data, ext_info.crop_rects);

                frame->frame_info->ext_info.emplace_back(ext_info);
            }

            output_image_(frame);
        } else if (format == ImageFormat::kPNG) {
            auto mem_obj = mem_pool_.alloc_mem_detach(0, 0);
            mem_obj->data = image_wrapper::image_png_dec(img_data.data(), img_data.size());

            auto frame = std::make_shared<msgs::cv_frame>(task_name_, TaskType::kAsyncImage, 1);
            frame->frame_info = std::make_shared<FrameInfo>(++frame_idx_, mem_obj);
            frame->response_callback_ = [ctx](const nlohmann::json &data, const bool code) {
                auto ack_json = nlohmann::json::object();
                ack_json["success"] = true;
                ack_json["data"] = data;
                ctx->send(ack_json.dump());
            };

            std::map<std::string, std::vector<std::vector<float>>> regions;
            if (additional.count("regions_with_label") > 0) {
                regions =
                    additional["regions_with_label"].get<std::map<std::string, std::vector<std::vector<float>>>>();
            }

            if (!regions.empty()) {
                // 构造一阶段检测的 ext_info
                nodes::FrameExtInfo ext_info(AlgoType::kDetection, "", "", 0);

                int index = 0;
                std::vector<Point2i> points;
                for (const auto &[key, values] : regions) {
                    std::vector<Point2i> points;
                    for (auto point : values) {
                        // 兼容比例和坐标
                        if (point[0] < 1) { point[0] = point[0] * frame->frame_info->width(); }
                        if (point[1] < 1) { point[1] = point[1] * frame->frame_info->height(); }
                        points.emplace_back(point[0], point[1]);
                    }
                    frame->frame_info->roi_points[key] = points;

                    auto x_compare = [](const Point2i &first, const Point2i &second) { return first.x < second.x; };
                    auto y_compare = [](const Point2i &first, const Point2i &second) { return first.y < second.y; };
                    float min_x = (*std::min_element(points.begin(), points.end(), x_compare)).x;
                    float min_y = (*std::min_element(points.begin(), points.end(), y_compare)).y;
                    float max_x = (*std::max_element(points.begin(), points.end(), x_compare)).x;
                    float max_y = (*std::max_element(points.begin(), points.end(), y_compare)).y;
                    float width = max_x - min_x;
                    float height = max_y - min_y;

                    if (width > height) {
                        min_y = min_y - (width - height) / 2;
                        height = width;
                    } else {
                        min_x = min_x - (height - width) / 2;
                        width = height;
                    }

                    ext_info.map_target_box.insert(std::make_pair(
                        index,
                        BoxInfo{.prev_id = 0,
                                .class_id = 0,
                                .prob = 1,
                                .box = {0, 0, (float)frame->frame_info->width(), (float)frame->frame_info->height()},
                                .roi_id = key,
                                .track_id = 0}));

                    ext_info.crop_rects[index] = algin_rect({min_x, min_y, width, height}, frame->frame_info->width(),
                                                            frame->frame_info->height(), scale_factor_);

                    ++index;
                }

                ext_info.flag_crop = true;
                ext_info.crop_images = image_wrapper::image_crop(mem_obj->data, ext_info.crop_rects);

                // index = 0;
                // for (const auto &item : ext_info.crop_images) {
                //     cv::Mat crop_image;
                //     item.second.download(crop_image);

                //     for (const auto &point : frame->frame_info->roi_points[ext_info.map_target_box[index].roi_id]) {
                //         cv::circle(crop_image,
                //                    {point.x - ext_info.crop_rects[index].x, point.y - ext_info.crop_rects[index].y}, 2,
                //                    cv::Scalar(0, 0, 255), -1);
                //     }

                //     cv::imwrite("output_" + std::to_string(index++) + ".jpg", crop_image);
                // }

                frame->frame_info->ext_info.emplace_back(ext_info);
            }

            output_image_(frame);
        } else {
            throw std::runtime_error("unsupport image format");
        }
    } catch (std::exception &exception) {
        auto ack_json = nlohmann::json::object();
        ack_json["success"] = false;
        ack_json["error"] = exception.what();
        ctx->send(ack_json.dump());
    }
}

void ImageServer_v2::on_response(const std::shared_ptr<msgs::cv_frame> &frame) {
    auto targets_obj = nlohmann::json::object();

    auto &ext_info = frame->frame_info->ext_info.back();
    if (ext_info.algo_type == AlgoType::kDetection || ext_info.algo_type == AlgoType::kClassification) {
        for (const auto &[idx, item] : ext_info.map_target_box) {
            auto object = nlohmann::json::object();
            object["bbox"]["label"] = ext_info.map_class_label.at(item.class_id);
            object["bbox"]["prob"] = int(item.prob * 10000 + 0.5) / 10000.0;
            object["bbox"]["color"] = ext_info.map_class_color.at(item.class_id).to_array();

            object["bbox"]["box"]["left_top_x"] = item.box.x;
            object["bbox"]["box"]["left_top_y"] = item.box.y;
            object["bbox"]["box"]["right_bottom_x"] = item.box.x + item.box.width;
            object["bbox"]["box"]["right_bottom_y"] = item.box.y + item.box.height;

            object["region_label"] = item.roi_id;

            targets_obj["targets"].push_back(object);
        }
    } else if (ext_info.algo_type == AlgoType::kClassification) {
        for (const auto &[idx, item] : ext_info.map_target_box) {
            auto object = nlohmann::json::object();
            object["bbox"]["label"] = ext_info.map_class_label.at(item.class_id);
            object["bbox"]["prob"] = int(item.prob * 10000 + 0.5) / 10000.0;
            object["bbox"]["color"] = ext_info.map_class_color.at(item.class_id).to_array();
            targets_obj["targets"]["bbox"].push_back(object);
        }
    } else if (ext_info.algo_type == AlgoType::kPose) {
        for (const auto &[idx, key_points] : ext_info.map_key_points) {
            auto object = nlohmann::json::object();
            const auto &target = ext_info.map_target_box.at(idx);
            object["bbox"]["label"] = ext_info.map_class_label.at(target.class_id);
            object["bbox"]["prob"] = int(target.prob * 10000 + 0.5) / 10000.0;
            object["bbox"]["color"] = ext_info.map_class_color.at(target.class_id).to_array();

            object["bbox"]["box"]["left_top_x"] = target.box.x;
            object["bbox"]["box"]["left_top_y"] = target.box.y;
            object["bbox"]["box"]["right_bottom_x"] = target.box.x + target.box.width;
            object["bbox"]["box"]["right_bottom_y"] = target.box.y + target.box.height;

            object["region_label"] = target.roi_id;

            for (const auto &point : key_points) {
                auto point_obj = nlohmann::json::object();
                point_obj["idx"] = point.number;
                point_obj["x"] = point.x;
                point_obj["y"] = point.y;
                point_obj["prob"] = point.prob;
                object["key_points"].push_back(point_obj);
            }

            if (key_points.size() == 5) {
                for (const auto &bone : skeleton) {
                    Scalar color;
                    if (bone[0] < 5 || bone[1] < 5) color = {0, 255, 0, 255};
                    else if (bone[0] > 12 || bone[1] > 12)
                        color = {255, 0, 0, 255};
                    else if (bone[0] > 4 && bone[0] < 11 && bone[1] > 4 && bone[1] < 11)
                        color = {0, 255, 255, 255};
                    else
                        color = {255, 0, 255, 255};

                    auto point_obj = nlohmann::json::object();
                    point_obj["start_x"] = key_points[bone[0]].x;
                    point_obj["start_y"] = key_points[bone[0]].y;
                    point_obj["end_x"] = key_points[bone[1]].x;
                    point_obj["end_y"] = key_points[bone[1]].y;
                    point_obj["color"] = color.to_array();
                    object["key_lines"].push_back(point_obj);
                }
            } else if (key_points.size() == 5) {
                for (const auto &bone : skeleton_5) {
                    auto point_obj = nlohmann::json::object();
                    point_obj["start_x"] = key_points[bone[0]].x;
                    point_obj["start_y"] = key_points[bone[0]].y;
                    point_obj["end_x"] = key_points[bone[1]].x;
                    point_obj["end_y"] = key_points[bone[1]].y;
                    point_obj["color"] = Scalar({0, 255, 0, 1}).to_array();
                    object["key_lines"].push_back(point_obj);
                }
            }

            targets_obj["targets"].push_back(object);
        }
    } else if (ext_info.algo_type == AlgoType::kOCR_REC || ext_info.algo_type == AlgoType::kOCR) {
        for (const auto &[idx, ocr_info] : ext_info.map_ocr_info) {
            auto box_obj = nlohmann::json::object();
            std::vector<cv::Point2f> points;
            for (const auto &point : ocr_info.points) { points.emplace_back(point.x, point.y); }
            auto rect = cv::boundingRect(points);

            float prob = 0;
            for (const auto &point : ocr_info.points) { prob += point.prob; }
            if (!ocr_info.points.empty()) { prob /= ocr_info.points.size(); }

            auto object = nlohmann::json::object();
            std::string label;
            for (auto &value : ocr_info.labels) { label += value.str; }
            object["bbox"]["label"] = label;
            object["bbox"]["prob"] = prob;
            object["bbox"]["color"] = std::array<int, 4>{255, 0, 0, 1};
            object["bbox"]["box"]["left_top_x"] = rect.x;
            object["bbox"]["box"]["left_top_y"] = rect.y;
            object["bbox"]["box"]["right_bottom_x"] = rect.x + rect.width;
            object["bbox"]["box"]["right_bottom_y"] = rect.y + rect.height;

            object["region_label"] = ocr_info.roi_id;

            targets_obj["targets"].push_back(object);
        }
    }

    /******************************** 分割 可视化 ****************************/
    for (const auto &[idx, contours] : ext_info.seg_contours) {
        for (const auto &item : contours) {
            auto object = nlohmann::json::object();
            auto seg_obj = nlohmann::json::object();

            seg_obj["area"] = item.area;
            seg_obj["length"] = item.length;
            seg_obj["volume"] = item.volume;

            if (item.volume >= 0) {
                object["bbox"]["label"] = "体积(cm^3)";
                object["bbox"]["prob"] = item.volume;
                object["bbox"]["color"] = std::array<int, 4>{255, 0, 0, 1};
                object["bbox"]["box"]["left_top_x"] = item.x;
                object["bbox"]["box"]["left_top_y"] = item.y;
                object["bbox"]["box"]["right_bottom_x"] = item.x;
                object["bbox"]["box"]["right_bottom_y"] = item.y;
            } else {
                object["bbox"]["label"] = "面积(cm^2)";
                object["bbox"]["prob"] = item.area;
                object["bbox"]["color"] = std::array<int, 4>{255, 0, 0, 1};
                object["bbox"]["box"]["left_top_x"] = item.x;
                object["bbox"]["box"]["left_top_y"] = item.y;
                object["bbox"]["box"]["right_bottom_x"] = item.x;
                object["bbox"]["box"]["right_bottom_y"] = item.y;
                targets_obj["targets"].push_back(object);

                int height = frame->frame_info->height();
                object["bbox"]["label"] = "长度(cm)";
                object["bbox"]["prob"] = item.length;
                object["bbox"]["color"] = std::array<int, 4>{255, 0, 0, 1};
                object["bbox"]["box"]["left_top_x"] = item.x;
                object["bbox"]["box"]["left_top_y"] = item.y + height * 0.05;
                object["bbox"]["box"]["right_bottom_x"] = item.x;
                object["bbox"]["box"]["right_bottom_y"] = item.y + height * 0.05;
                targets_obj["targets"].push_back(object);
            }

            targets_obj["geometries"][ext_info.map_class_label.at(idx)].emplace_back(seg_obj);
            targets_obj["targets"].push_back(object);
        }
    }
    /***********************************************************************/

    for (const auto &item : ext_info.mosaic_rects) {
        auto mosaic_obj = nlohmann::json::object();
        mosaic_obj["left_top_x"] = item.x;
        mosaic_obj["left_top_y"] = item.y;
        mosaic_obj["right_bottom_x"] = item.x + item.width;
        mosaic_obj["right_bottom_y"] = item.y + item.height;
        mosaic_obj["color"] = {255, 255, 255, 1};
        targets_obj["mosaics"].emplace_back(std::move(mosaic_obj));
    }

    targets_obj["metadata"] = ext_info.metadata;

    frame->response_callback_(targets_obj, true);
}

}// namespace nodes
}// namespace gddi
