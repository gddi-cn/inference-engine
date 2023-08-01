/**
 * @file test_cross_border.cpp
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022 GDDI
 * 
 */

#include "modules/bytetrack/target_tracker.h"
#include "modules/postprocess/cross_border.h"
#include <spdlog/spdlog.h>
#include "types.hpp"
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

class CrossBorderTest : public testing::Test {
protected:
    virtual void SetUp() override {
        printf("Enter into SetUp\n");

        spdlog::set_level(spdlog::level::debug);

        // 跟踪 + 越界
        tracker_ = std::make_unique<gddi::TargetTracker>(
            gddi::TrackOption{.track_thresh = 0.5, .high_thresh = 0.6, .match_thresh = 0.8});
        counter_ = std::make_unique<gddi::CrossBorder>();

        // 初始化边界, 规定从左到右穿过射线视为越界, 这里假设越过中间线
        std::vector<gddi::Point2i> border{{int(width_ / 2), int(height_)}, {int(width_ / 2), 0}};
        counter_->init_border(border, margin_);

        // 当前位置
        gddi::Rect cur_rect{0, height_ / 2, 50, 100};

        // 边界右侧隔离区 margin_
        while ((++cur_rect.x) <= width_ / 2 + margin_ - 25) {
            // 存放检测出的目标信息
            auto vec_objects = std::vector<gddi::TrackObject>();
            vec_objects.push_back(gddi::TrackObject{
                .target_id = 0,
                .class_id = 0,
                .prob = 0.9,
                .rect = {(float)cur_rect.x, (float)cur_rect.y, (float)cur_rect.width, (float)cur_rect.height}});

            // 目标跟踪
            std::map<int, gddi::Rect2f> rects;
            auto tracked_target = tracker_->update_objects(vec_objects);
            for (auto &[track_id, target] : tracked_target) {
                rects[target.target_id] =
                    gddi::Rect2f{(float)cur_rect.x, (float)cur_rect.y, (float)cur_rect.width, (float)cur_rect.height};
            }

            // 更新目标位置，若 vec_direction 非空，则有新目标越界
            auto vec_direction = counter_->update_position(rects);
            ASSERT_EQ(vec_direction[0].size(), 0);
        };

        auto vec_objects = std::vector<gddi::TrackObject>();
        vec_objects.push_back(gddi::TrackObject{
            .target_id = 0,
            .class_id = 0,
            .prob = 0.9,
            .rect = {(float)cur_rect.x, (float)cur_rect.y, (float)cur_rect.width, (float)cur_rect.height}});

        // 目标跟踪
        std::map<int, gddi::Rect2f> rects;
        auto tracked_target = tracker_->update_objects(vec_objects);
        for (auto &[track_id, target] : tracked_target) {
            rects[target.target_id] =
                gddi::Rect2f{(float)cur_rect.x, (float)cur_rect.y, (float)cur_rect.width, (float)cur_rect.height};
        }

        // 更新目标位置，若 vec_direction 非空，则有新目标越界
        auto vec_direction = counter_->update_position(rects);
        ASSERT_EQ(vec_direction[0].size(), 1);
        printf("============= border: [%d, %d, %d, %d], margin: %d, cur_x: %d, cur_y: %d, cur_w: %d, cur_h: %d\n",
               border[0].x, border[0].y, border[1].x, border[1].y, margin_, cur_rect.x, cur_rect.y, cur_rect.width,
               cur_rect.height);
    }

    virtual void TearDown() override { printf("Exit from TearDown\n"); }

    int width_ = 1920;
    int height_ = 1080;
    int margin_ = 20;
    float mod_thres_ = 0.6;// 模型阈值
    int max_lost_time_ = 4;// 4s
    int frame_rate_ = 25;  // 检测帧率
    int class_num_ = 10;   // 模型标签类别数

    std::unique_ptr<gddi::TargetTracker> tracker_;
    std::unique_ptr<gddi::CrossBorder> counter_;
};

TEST_F(CrossBorderTest, DefaultConstructor) {}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}