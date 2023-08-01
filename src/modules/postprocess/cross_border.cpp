#include "cross_border.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace gddi {

enum class Direction { kLeft = 0, kRight, kMiddle };

class CrossBorderPrivate {
public:
    CrossBorderPrivate() {}
    ~CrossBorderPrivate() {}

    bool init_border(const std::vector<Point2i> &border, const int margin) {
        border_ = border;
        margin_ = margin;

        if (border_.size() != 2) {
            printf("Line type border vertex number should be equal to 2, actually %ld", border.size());
            return false;
        }

        return true;
    }

    std::map<int, int> update_line(const std::map<int, Rect2f> &rects) {
        std::map<int, int> result;

        for (const auto &[track_id, rect] : rects) {
            auto &track_id_status = target_status_[track_id];
            last_update_time_[track_id] = time(NULL);

            float distance =
                calculate_distance(Point2i{int(rect.x + rect.width / 2), int(rect.y + rect.height / 2)}, border_);

            if (track_id_status.size() > 150) { track_id_status.erase(track_id_status.begin()); }

            spdlog::debug("================ {}", distance);
            if (distance >= 0) {
                track_id_status.emplace_back(
                    direction(Point2i{int(rect.x + rect.width / 2), int(rect.y + rect.height / 2)}, border_));

                int len = track_id_status.size();
                for (int i = 0; i < len - 1; i++) {
                    if (track_id_status[i] == Direction::kLeft) {
                        if (std::find_if(track_id_status.begin() + i + 1, track_id_status.end(),
                                         [](const Direction &direction) { return direction == Direction::kRight; })
                            != track_id_status.end()) {
                            result.insert(std::make_pair(track_id, 0));
                            track_id_status.clear();
                            break;
                        }
                    } else if (track_id_status[i] == Direction::kRight) {
                        if (std::find_if(track_id_status.begin() + i + 1, track_id_status.end(),
                                         [](const Direction &direction) { return direction == Direction::kLeft; })
                            != track_id_status.end()) {
                            result.insert(std::make_pair(track_id, 1));
                            track_id_status.clear();
                            break;
                        }
                    }
                }
            }
        }

        // 移除丢失目标
        for (auto iter = last_update_time_.begin(); iter != last_update_time_.end();) {
            if (time(NULL) - iter->second > 60) {
                iter = last_update_time_.erase(iter);
                target_status_.erase(iter->first);
            } else {
                ++iter;
            }
        }

        return result;
    }

protected:
    float distance_to_line(const Point2i &point, const std::vector<Point2i> &line) {
        float k = (line[1].y - line[0].y) * 1.0 / (line[1].x - line[0].x);
        float b = line[0].y - k * line[0].x;
        return abs(k * point.x - point.y + b) / sqrt(k * k + 1);
    }

    float calculate_distance(const Point2i &point, const std::vector<Point2i> &line) {
        std::vector<int> ap = {point.x - line[0].x, point.y - line[0].y};
        std::vector<int> ab = {line[1].x - line[0].x, line[1].y - line[0].y};
        float len_ap = sqrt(ap[0] * ap[0] + ap[1] * ap[1]);
        float len_ab = sqrt(ab[0] * ab[0] + ab[1] * ab[1]);
        float dot_product = ap[0] * ab[0] + ap[1] * ab[1];
        float distance = abs(ap[0] * ab[1] - ap[1] * ab[0]) / len_ab;
        if (dot_product < 0 || dot_product > len_ab * len_ab) { distance = -distance; }
        return distance;
    }

    Direction direction(const Point2i &point, const std::vector<Point2i> &line) {
        // 射线的方向向量OA
        std::vector<int> oa = {line[1].x - line[0].x, line[1].y - line[0].y};
        // 向量OP
        std::vector<int> op = {point.x - line[0].x, point.y - line[0].y};
        // 向量OP和OA的叉积
        int cross_product = op[0] * oa[1] - op[1] * oa[0];
        if (calculate_distance(point, line) <= margin_) {
            return Direction::kMiddle;
        } else if (cross_product > 0) {
            return Direction::kLeft;
        } else {
            return Direction::kRight;
        }
    }

private:
    int margin_{0};
    std::vector<Point2i> border_;

    std::map<int, std::vector<Direction>> target_status_;
    std::map<int, time_t> last_update_time_;
};

CrossBorder::CrossBorder() {}

CrossBorder::~CrossBorder() {}

bool CrossBorder::init_border(const std::vector<Point2i> &border, const int margin) {
    cross_border_.resize(1);
    return cross_border_[0].init_border(border, margin);
}

bool CrossBorder::init_border(const std::vector<std::vector<Point2i>> &borders, const int margin) {
    for (const auto &item : borders) {
        CrossBorderPrivate crosser;
        if (crosser.init_border(item, margin)) {
            cross_border_.emplace_back(std::move(crosser));
        } else {
            return false;
        }
    }
    return true;
}

std::vector<std::map<int, int>> CrossBorder::update_position(const std::map<int, Rect2f> &rects) {
    std::vector<std::map<int, int>> cross_count;
    for (auto &item : cross_border_) { cross_count.push_back(item.update_line(rects)); }
    return cross_count;
}

}// namespace gddi