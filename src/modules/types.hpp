/**
 * @file types.hpp
 * @author zhdotcai
 * @brief 
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022 GDDI
 * 
 */

#ifndef __GDDI_TYPES_HPP__
#define __GDDI_TYPES_HPP__

#include <array>
#include <stddef.h>

namespace gddi {

template<typename _Tp>
class Rect_ {
public:
    size_t area() const { return width * height; }

public:
    _Tp x;
    _Tp y;
    _Tp width;
    _Tp height;
};

template<typename _Tp>
class Scalar_ {
public:
    _Tp r;
    _Tp g;
    _Tp b;
    _Tp a;

    std::array<_Tp, 4> to_array() const { return std::array<_Tp, 4>({r, g, b, a}); }

    template<typename _Up>
    std::array<_Up, 4> to_array() const {
        return std::array<_Up, 4>({static_cast<_Up>(r), static_cast<_Up>(g), static_cast<_Up>(b), static_cast<_Up>(a)});
    }
};

template<typename _Tp>
class Point2_ {
public:
    Point2_(_Tp xx, _Tp yy) : x(xx), y(yy) {}

public:
    _Tp x;
    _Tp y;
};

using Rect = Rect_<int>;
using Rect2f = Rect_<float>;
using Scalar = Scalar_<float>;
using Point2i = Point2_<int>;
using Point2f = Point2_<float>;

struct Keypoint {
    float x;
    float y;
    float prob;
};

}// namespace gddi

#endif