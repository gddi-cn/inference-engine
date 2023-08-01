/**
 * @file cross_border.h
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022 GDDI
 * 
 */

#ifndef __CROSS_BORDER_H__
#define __CROSS_BORDER_H__

#include "types.hpp"
#include <map>
#include <memory>
#include <vector>

namespace gddi {

class CrossBorderPrivate;

class CrossBorder {
public:
    /**
     * @brief Construct a new Cross Border object
     * 
     * @param margin 
     */
    CrossBorder();
    ~CrossBorder();

    /**
     * @brief init boder
     * 
     * @param border ray
     * @return true 
     * @return false 
     */
    bool init_border(const std::vector<Point2i> &border, const int margin = 0);

    /**
     * @brief init boder
     * 
     * @param border ray
     * @return true 
     * @return false 
     */
    bool init_border(const std::vector<std::vector<Point2i>> &borders, const int margin = 0);

    /**
     * @brief Update position
     * 
     * @param rects track_id -> track_rect
     * @return there are new goals out of bounds, if not null
     */
    std::vector<std::map<int, int>> update_position(const std::map<int, Rect2f> &rects);

private:
    std::vector<CrossBorderPrivate> cross_border_;
};

}// namespace gddi

#endif