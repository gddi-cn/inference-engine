#include "feature_library.h"
#include <boost/filesystem.hpp>
#include <cstdint>
#include <opencv2/imgcodecs.hpp>
#include <unordered_map>

extern "C" {
#include <libavutil/frame.h>
}

namespace gddi {
namespace nodes {

FeatureLibrary_v2::~FeatureLibrary_v2() {}

void FeatureLibrary_v2::on_setup() {
    // 获取目录下所有的图片文件
    get_image_files(path_suffix_ + folder_path_);
    // 读取特征库信息
    load_feature_v1(path_suffix_ + folder_path_);

    int index = 0;
    for (const auto &[file, _] : file_list_) {
        auto image = cv::imread(file);
        if (image.empty()) { continue; }

#if defined(WITH_JETSON)
        auto mem_obj = mem_pool_.alloc_mem_detach(0, 0);
#elif defined(WITH_BM1684)
        auto mem_obj = mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(nullptr, FORMAT_BGR_PLANAR, 0, 0, false);
#elif defined(WITH_MLU220) || defined(WITH_MLU270)
        auto mem_obj = mem_pool_.alloc_mem_detach<std::shared_ptr<AVFrame>>(nullptr, CNCODEC_PIX_FMT_NV12, 0, 0);
#elif defined(WITH_MLU370)
        auto mem_obj = mem_pool_.alloc_mem_detach();
#else
        auto mem_obj = mem_pool_.alloc_mem_detach(0, 0);
#endif

        mem_obj->data = image_wrapper::mat_to_image(image);
        auto frame = std::make_shared<msgs::cv_frame>(file, TaskType::kAsyncImage, 1);
        frame->frame_info = std::make_shared<FrameInfo>(++index, mem_obj);
        output_image_(frame);
    }

    auto frame = std::make_shared<msgs::cv_frame>(task_name_, TaskType::kAsyncImage, 1, gddi::FrameType::kNone);
    output_image_(frame);
}

void FeatureLibrary_v2::on_response(const std::shared_ptr<msgs::cv_frame> &frame) {
    if (frame->frame_type == gddi::FrameType::kNone) {
        save_feature_v1(path_suffix_ + folder_path_);
        quit_runner_(TaskErrorCode::kFeatureLibrary);
        return;
    }

    auto &ext_info = frame->frame_info->ext_info.back();
    if (ext_info.algo_type == AlgoType::kFace) {
        if (ext_info.features.size() == 1) {
            auto iter = std::find_if(feature_info_.begin(), feature_info_.end(),
                                     [&frame](const FeatureInfo_v1 &info) { return info.path == frame->task_name; });
            if (iter != feature_info_.end()) {
                memcpy(iter->feature, ext_info.features[0].data(), ext_info.features[0].size());
            } else {
                FeatureInfo_v1 info;
                memset(&info, 0, sizeof(info));
                memcpy(info.path, frame->task_name.c_str() + 11, frame->task_name.size() - 11 + 1);
                auto &sha256 = file_list_[frame->task_name];
                memcpy(info.sha256, sha256.data(), sha256.size());
                auto &feature = ext_info.features.begin()->second;
                memcpy(info.feature, feature.data(), feature.size() * sizeof(float));
                feature_info_.emplace_back(std::move(info));
            }
        }
    }
}

void FeatureLibrary_v2::get_image_files(const std::string &path) {
    if (boost::filesystem::is_directory(path)) {
        boost::filesystem::recursive_directory_iterator end_iter;
        boost::filesystem::recursive_directory_iterator dir_iter(path);
        while (dir_iter != end_iter) {
            if (boost::filesystem::is_regular_file(dir_iter->status())) {
                if (dir_iter->path().filename().string() == "cover.jpg") {
                    ++dir_iter;
                    continue;
                }

                // 判断文件是否是图片
                auto ext = dir_iter->path().extension().string();
                if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
                    utils::sha256sum(dir_iter->path().string(), file_list_[dir_iter->path().string()]);
                }

            } else if (boost::filesystem::is_directory(dir_iter->status())) {
                get_image_files(dir_iter->path().c_str());
            }
            ++dir_iter;
        }
    }
}

void FeatureLibrary_v2::load_feature_v1(const std::string &path) {
    rename((path + "/features.bin").c_str(), (path + "/.features.bin.bak").c_str());
    rename((path + "/features.txt").c_str(), (path + "/.features.txt.bak").c_str());

    std::ifstream ifs(path + "/.features.bin.bak", std::ios::binary);
    if (ifs.is_open()) {
        ifs.seekg(0, std::ios::end);
        size_t size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        uint32_t verison;
        ifs.read(reinterpret_cast<char *>(&verison), sizeof(verison));

        feature_info_.resize((size - sizeof(uint32_t)) / sizeof(FeatureInfo_v1));
        ifs.read(reinterpret_cast<char *>(feature_info_.data()), size);
        ifs.close();
    }

    for (auto iter = feature_info_.begin(); iter != feature_info_.end();) {
        auto file_iter = std::find_if(file_list_.begin(), file_list_.end(),
                                      [&iter](const std::pair<std::string, std::vector<uint8_t>> &file) {
                                          return memcmp(iter->sha256, file.second.data(), sizeof(iter->sha256)) == 0;
                                      });

        auto relative_path = file_iter->first.c_str() + 11;
        if (file_iter == file_list_.end()) {
            spdlog::info("Remove file: {}", iter->path);
            iter = feature_info_.erase(iter);
        } else {
            if (strcmp(iter->path, relative_path) != 0) {
                // 文件名不一致，更新文件名
                spdlog::info("Update file name: {} -> {}", iter->path, relative_path);
                memcpy(iter->path, relative_path, strlen(relative_path) + 1);
            }
            file_list_.erase(file_iter);
            ++iter;
        }
    }
}

void FeatureLibrary_v2::save_feature_v1(const std::string &path) {
    // 保存特征库信息
    std::ofstream ofs(path + "/.features.bin.tmp", std::ios::binary | std::ios::trunc);
    if (ofs.is_open()) {
        uint32_t version = 1;
        ofs.write(reinterpret_cast<const char *>(&version), sizeof(version));
        ofs.write(reinterpret_cast<const char *>(feature_info_.data()), feature_info_.size() * sizeof(FeatureInfo_v1));
        ofs.close();
    }

    // 保存成功的图片路径
    std::ofstream ofs_success(path + "/.features.txt.tmp", std::ios::trunc);
    if (ofs_success.is_open()) {
        for (const auto &item : feature_info_) { ofs_success << item.path << std::endl; }
        ofs_success.close();
    }

    rename((path + "/.features.bin.tmp").c_str(), (path + "/features.bin").c_str());
    rename((path + "/.features.txt.tmp").c_str(), (path + "/features.txt").c_str());
}

}// namespace nodes
}// namespace gddi