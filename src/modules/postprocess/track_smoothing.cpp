#include "track_smoothing.h"
#include "basic_logs.hpp"
#include <Eigen/Dense>
#include <memory>
#include <vector>

namespace gddi {

class SmoothingPrivate {
public:
    virtual ~SmoothingPrivate() = default;
    virtual void smooth_keypoints(std::vector<nodes::PoseKeyPoint> &keypoints) = 0;
};

class KalmanFilter : public SmoothingPrivate {
public:
    KalmanFilter(int keypoint_count) {
        int state_dim = 4 * keypoint_count;
        int meas_dim = 2 * keypoint_count;

        // Initialize Kalman filter
        F_ = Eigen::MatrixXf::Identity(state_dim, state_dim);
        H_ = Eigen::MatrixXf::Identity(meas_dim, state_dim);
        Q_ = Eigen::MatrixXf::Identity(state_dim, state_dim) * 1e-4;
        R_ = Eigen::MatrixXf::Identity(meas_dim, meas_dim) * 1e-1;

        state_ = Eigen::VectorXf::Zero(state_dim);
        P_ = Eigen::MatrixXf::Identity(state_dim, state_dim) * 1e-1;
    }

    void smooth_keypoints(std::vector<nodes::PoseKeyPoint> &keypoints) {
        Eigen::VectorXf measurement(keypoints.size() * 2);

        for (int i = 0; i < keypoints.size(); i++) {
            state_(i * 2) = keypoints[i].x;
            state_(i * 2 + 1) = keypoints[i].y;
            measurement(i * 2) = keypoints[i].x;
            measurement(i * 2 + 1) = keypoints[i].y;
        }

        Eigen::VectorXf prediction = predict();
        correct(measurement);

        for (int i = 0; i < keypoints.size(); i++) {
            keypoints[i].x = prediction(i * 2);
            keypoints[i].y = prediction(i * 2 + 1);
        }
    }

private:
    Eigen::VectorXf state_;
    Eigen::MatrixXf F_, H_, Q_, R_, P_;

    Eigen::VectorXf predict() {
        state_ = F_ * state_;
        P_ = F_ * P_ * F_.transpose() + Q_;
        return state_;
    }

    void correct(const Eigen::VectorXf &measurement) {
        Eigen::MatrixXf K = P_ * H_.transpose() * (H_ * P_ * H_.transpose() + R_).inverse();
        state_ = state_ + K * (measurement - H_ * state_);
        P_ = (Eigen::MatrixXf::Identity(state_.size(), state_.size()) - K * H_) * P_;
    }
};

class MedianFilter : public SmoothingPrivate {
public:
    MedianFilter(int kernel_size = 3) : kernel_size_(kernel_size) {}

    void smooth_keypoints(std::vector<nodes::PoseKeyPoint> &keypoints) override {
        cache_keypoints_.emplace_back(keypoints);
        if (cache_keypoints_.size() < kernel_size_) { return; }

        for (int i = 0; i < keypoints.size(); i++) {
            std::vector<float> x_data;
            std::vector<float> y_data;
            for (int j = 0; j < kernel_size_; j++) {
                x_data.emplace_back(cache_keypoints_[0][i].x);
                y_data.emplace_back(cache_keypoints_[0][i].y);
            }

            std::sort(x_data.begin(), x_data.end());
            std::sort(y_data.begin(), y_data.end());

            spdlog::info("x: {} -> x: {}, y: {} -> y: {}", keypoints[i].x, keypoints[i].y, x_data[kernel_size_ / 2],
                         y_data[kernel_size_ / 2]);

            keypoints[i].x = x_data[kernel_size_ / 2];
            keypoints[i].y = y_data[kernel_size_ / 2];
        }

        cache_keypoints_.erase(cache_keypoints_.begin());
    }

private:
    int kernel_size_;
    std::vector<std::vector<nodes::PoseKeyPoint>> cache_keypoints_;
};

TrackSmoothing::TrackSmoothing(const SmoothingType type) {
    if (type == SmoothingType::kMedian) {
        up_impl_ = std::make_unique<MedianFilter>(3);
    } else if (type == SmoothingType::kKalman) {
        up_impl_ = std::make_unique<KalmanFilter>(17);
    }
}

void TrackSmoothing::smooth_keypoints(std::vector<nodes::PoseKeyPoint> &keypoints) {
    up_impl_->smooth_keypoints(keypoints);
}

TrackSmoothing::~TrackSmoothing() {}

}// namespace gddi