/**
 * @file test_angle_calculator.cpp
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 0.1
 * @date 2023-04-05
 * 
 * @copyright Copyright (c) 2023 by GDDI
 * 
 */

#include "modules/postprocess/angle_calculator.h"
#include <gtest/gtest.h>
#include <memory>
#include <spdlog/spdlog.h>

class AngleCalculatorTest : public testing::Test {
protected:
    virtual void SetUp() override {
        printf("Enter into SetUp\n");

        spdlog::set_level(spdlog::level::debug);

        std::vector<cv::Point2f> roi_points{{389.13, 133.233},
                                            {392.422, 375.619},
                                            {231.105, 431.586},
                                            {98.5947, 303.603},
                                            {148.8, 131.175}};

        std::vector<cv::Point2f> pointer_points{{172.669, 303.191}, {122.874, 330.352}};
        cv::Mat input_points(pointer_points, true);

        auto calculator = std::make_unique<AngleCalculator>();
        calculator->set_roi_points(roi_points);
        float ratio = calculator->get_ratio(input_points);

        ASSERT_TRUE(std::fabs(ratio - 0.730241) < 1e-6f);
    }

    virtual void TearDown() override { printf("Exit from TearDown\n"); }
};

TEST_F(AngleCalculatorTest, DefaultConstructor) {}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}