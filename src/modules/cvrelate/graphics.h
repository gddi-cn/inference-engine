#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "modules/types.hpp"
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>

namespace gddi {
namespace graphics {
class Graphics {
public:
    Graphics();
    ~Graphics();

    /**
         * @brief 初始化图像
         * 
         * @param image 
         * @return true 
         * @return false 
         */
    bool set_image(const cv::Mat &image);

    /**
     * @brief 获取处理后数据帧
     * 
     * @return cv::Mat 
     */
    cv::Mat get_image();

    /**
     * @brief 设置字体
     * 
     * @param font_face 字体路径
     * @return true 
     * @return false 
     */
    bool set_font_face(const std::string &font_face);

    /**
     * @brief 绘制矩形框
     * 
     * @param rect  x, y, width, height
     * @param prop  阈值，默认 0.0f 不绘制
     * @param width 矩形框厚度
     * @return bool
     */
    bool draw_rect(const Rect2f &rect, const float prob = 0.0f, const Scalar &color = {255, 255, 255},
                   double width = 4);

    /**
     * @brief 绘制矩形框，填充背景
     * 
     * @param rect  x, y, width, height
     * @param width 矩形框厚度
     * @return bool
     */
    bool draw_rect_fill(const Rect2f &rect, const Scalar &color = {255, 255, 255, 0});

    /**
     * @brief 绘制多边形，填充背景
     * 
     * @param rect  x, y, width, height
     * @param width 矩形框厚度
     * @return bool
     */
    bool draw_polygon_fill(const std::vector<Point2i> &points, const Scalar &color = {255, 255, 255});

    /**
     * @brief 绘制文字
     * 
     * @param pos   绘制坐标起点
     * @param text  文本
     * @param font_size 文本大小
     * @return bool
     */
    bool draw_text(const Point2i &pos, const std::string &text, const Scalar &color = {255, 255, 255},
                   float font_size = 20.0f);

    /**
     * @brief 绘制文字，背景填充，白色字体
     * 
     * @param pos   绘制坐标起点
     * @param text  文本
     * @param font_size 文本大小
     * @return bool
     */
    bool draw_text_fill(const Point2i &pos, const std::string &text, const Scalar &color = {255, 255, 255},
                        float font_size = 16.0f);

    /**
     * @brief 绘制点，背景填充白色
     *
     * @param pos   绘制坐标起点
     * @param text  文本
     * @param font_size 文本大小
     * @return bool
     */
    bool draw_circle(const Point2i &pos, const Scalar &color = {255, 255, 255}, float size = 20.0f);

    /**
     * @brief 绘制，背景填充白色
     *
     * @param pos   绘制坐标起点
     * @param text  文本
     * @param font_size 文本大小
     * @return bool
     */
    bool draw_line(const Point2i &spos, const Point2i &e_pos, const Scalar &color = {255, 255, 255},
                   float width = 20.0f);

    /**
     * @brief 
     * 
     * @param rect 裁切图像框信息
     * @return cv::Mat 
     */
    cv::Mat crop_image(Rect2f rect);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}// namespace graphics
}// namespace gddi

#endif