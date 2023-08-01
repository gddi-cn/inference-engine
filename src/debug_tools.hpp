//
// Created by cc on 2021/11/1.
//

#ifndef FFMPEG_WRAPPER_SRC_DEBUG_TOOLS_HPP_
#define FFMPEG_WRAPPER_SRC_DEBUG_TOOLS_HPP_

#include <chrono>
#include <ratio>
#include "utils.hpp"

template<class Ty>
inline
void DEBUG_release_view(Ty obj_val) {
    std::cout << "Release: (" << gddi::utils::get_class_name(&obj_val) << "), " << obj_val << std::endl;
}

template<class Ty>
inline
void DEBUG_release_view(Ty *obj_val) {
    std::cout << "Release: (" << gddi::utils::get_class_name(&obj_val) << "), " << obj_val << std::endl;
}

namespace DebugTools {

template<class Func, class...Args>
double metrics_ms(Args ...args) {
    auto begin_ = std::chrono::high_resolution_clock::now();
    Func(std::forward<Args>(args)...);
    auto end_ = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end_ - begin_).count() * 1000;
}

class TickMetrics {
public:
    void begin() {
        tick_begin_ = std::chrono::high_resolution_clock::now();
    }

    std::chrono::high_resolution_clock::time_point::duration elapsedTicks() const {
        auto end_ = std::chrono::high_resolution_clock::now();
        return (end_ - tick_begin_);
    }

    double elapsedMilliseconds() const {
        return std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - tick_begin_).count();
    }

protected:
    std::chrono::high_resolution_clock::time_point tick_begin_;
};

inline void quit_on_cin_char(char quit_ch = 'q') {
    char cmd_ch;
    while (true) {
        std::cin >> cmd_ch;
        if (cmd_ch == quit_ch) {
            break;
        }
    }
}

}

#endif //FFMPEG_WRAPPER_SRC_DEBUG_TOOLS_HPP_
