#include "json.hpp"
#include "modules/postprocess//fft_counter.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

int main() {
    std::fstream key_points_file("key_point.json");
    std::string buffer((std::istream_iterator<char>(key_points_file)), std::istream_iterator<char>());
    auto json_obj = nlohmann::json::parse(buffer);

    std::fstream fft_points_file("fft_point.json");
    std::string fft_buffer((std::istream_iterator<char>(fft_points_file)), std::istream_iterator<char>());
    auto fft_json_obj = nlohmann::json::parse(fft_buffer);

    std::map<int, gddi::PosePoint> pose_points;
    for (const auto &[type, value] : fft_json_obj.items()) {
        for (const auto &[key, item] : value.items()) {
            gddi::PosePoint pose_point;
            pose_point.weight = item["weight"].get<double>();
            auto orientation = item["orientation"].get<std::string>();
            if (orientation == "vertical") {
                pose_point.orientation = gddi::Orientation::kVertical;
            } else if (orientation == "horizontal") {
                pose_point.orientation = gddi::Orientation::kHorizontal;
            } else if (orientation == "score") {
                pose_point.orientation = gddi::Orientation::kScore;
            }
            pose_points.insert(std::make_pair(atoi(key.c_str()), std::move(pose_point)));
        }
    }

    std::vector<std::vector<gddi::nodes::PoseKeyPoint>> standard_range;

    std::fstream standard_points_file("standard.json");
    std::string standard_buffer((std::istream_iterator<char>(standard_points_file)), std::istream_iterator<char>());
    auto standard_json_obj = nlohmann::json::parse(standard_buffer);

    int index = 0;
    for (const auto &item : standard_json_obj) {
        for (const auto &key_points : item) {
            index = 0;
            std::vector<gddi::nodes::PoseKeyPoint> vec_key_point;
            for (const auto &key_point : key_points) {
                auto values = key_point.get<std::vector<float>>();
                gddi::nodes::PoseKeyPoint pose_key_point;
                pose_key_point.number = index++;
                pose_key_point.x = values[0];
                pose_key_point.y = values[1];
                pose_key_point.prob = values[2];
                vec_key_point.emplace_back(std::move(pose_key_point));
            }
            standard_range.emplace_back(std::move(vec_key_point));
        }
    }

    index = 0;
    auto fft_couter = std::make_unique<gddi::FFTCounter>(pose_points, 0);
    auto action_pair = std::make_unique<gddi::ActionPair>(pose_points);

    int action_index = 1;
    for (const auto &item : json_obj) {
        std::vector<gddi::nodes::PoseKeyPoint> key_points;
        for (const auto &key_point : item) {
            auto values = key_point.get<std::vector<float>>();
            key_points.emplace_back(gddi::nodes::PoseKeyPoint{.number = (int)key_points.size(),
                                                              .x = values[0],
                                                              .y = values[1],
                                                              .prob = values[2]});
        }

        int num = fft_couter->update(0, index, key_points);
        printf("========== frame_id: %d, num %d\n", index++, num);

        std::map<int64_t, std::vector<gddi::nodes::PoseKeyPoint>> action_key_points;
        fft_couter->get_range(0, action_key_points);

        if (!action_key_points.empty()) {
            // std::map<std::pair<int, int>, std::vector<double>> pairs;
            // double score = action_pair->pair_action(action_key_points, standard_range, pairs);
            // printf("两个动作的距离是 %f, 该得分越低越好\n", score);
            // for (const auto &[key, values] : pairs) {
            //     printf("\t第%d次动作, 第%d帧和第%d帧匹配\n", action_index, key.first, key.second);

            //     int value_index = 0;
            //     for (const auto &value : values) { printf("\t\t关键点%d的得分是%f\n", value_index++, value); }
            // }
            action_index++;
        }
    }

    // auto start = std::chrono::steady_clock::now();

    // index = 1;
    // for (const auto &item : result) {
    //     std::map<std::pair<int, int>, std::vector<double>> pairs;
    //     double score = action_pair->pair_action(action_key_points, standard_range, pairs);
    //     printf("两个动作的距离是 %f, 该得分越低越好\n", score);
    //     for (const auto &[key, values] : pairs) {
    //         printf("\t第%d次动作, 第%d帧和第%d帧匹配\n", index, key.first, key.second);

    //         int value_index = 0;
    //         for (const auto &value : values) {
    //             printf("\t\t关键点%d的得分是%f\n", value_index++, value);
    //         }
    //     }
    //     index++;
    // }

    // printf("====================== %dms\n",
    //        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
    //                                                              - start)
    //            .count());
}