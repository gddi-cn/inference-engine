#include "graphics.h"
#include "json.hpp"
#include "utils.hpp"
#include <blend2d.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace gddi {
namespace graphics {

struct Graphics::Impl {
    cv::Mat bgra;// 存储当前 rgb 图像

    BLImage img;
    BLContext ctx;
    BLFontFace face;
};

Graphics::Graphics() : impl_(std::make_unique<Impl>()) {}

Graphics::~Graphics() {}

bool Graphics::set_image(const cv::Mat &image) {
    if (image.channels() == 3) {
        cv::cvtColor(image, impl_->bgra, cv::COLOR_BGR2BGRA);
    } else {
        impl_->bgra = image.clone();
    }

    if (impl_->img.createFromData(impl_->bgra.cols, impl_->bgra.rows, BL_FORMAT_XRGB32, impl_->bgra.data,
                                  impl_->bgra.step1())
        != BL_SUCCESS) {
        return false;
    }

    impl_->ctx = BLContext(impl_->img);

    return true;
}

cv::Mat Graphics::get_image() {
    impl_->ctx.end();
    return impl_->bgra;
}

bool Graphics::set_font_face(const std::string &font_face) {
    if (impl_->face.createFromFile(font_face.c_str()) != BL_SUCCESS) { return false; }
    return true;
}

bool Graphics::draw_rect(const Rect2f &rect, const float prob, const Scalar &color, double width) {
    impl_->ctx.setStrokeStyle(BLRgba32(color.r, color.g, color.b, color.a));
    impl_->ctx.setStrokeWidth(width);
    impl_->ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);

    return true;
}

bool Graphics::draw_rect_fill(const Rect2f &rect, const Scalar &color) {
    impl_->ctx.setFillStyle(BLRgba32(color.r, color.g, color.b, color.a));
    impl_->ctx.setStrokeWidth(2);
    impl_->ctx.fillRect(rect.x, rect.y - 15, rect.width, rect.height);

    return true;
}

bool Graphics::draw_polygon_fill(const std::vector<Point2i> &points, const Scalar &color) {
    auto pointi = std::vector<BLPointI>(points.size());
    BLArrayView<BLPointI> poly;
    for (size_t i = 0; i < points.size(); i++) {
        pointi[i].x = points[i].x;
        pointi[i].y = points[i].y;
    }
    poly.reset(pointi.data(), pointi.size());

    impl_->ctx.setStrokeStyle(BLRgba32(color.r, color.g, color.b, color.a));
    impl_->ctx.fillPolygon(poly);

    return true;
}

bool Graphics::draw_text(const Point2i &pos, const std::string &text, const Scalar &color, float font_size) {
    BLFont font;
    font.createFromFace(impl_->face, font_size);

    impl_->ctx.setFillStyle(BLRgba32(color.r, color.g, color.b, color.a));
    impl_->ctx.fillUtf8Text(BLPoint(pos.x, pos.y), font, text.c_str());

    return true;
}

bool Graphics::draw_text_fill(const Point2i &pos, const std::string &text, const Scalar &color, float font_size) {
    BLFont font;
    font.createFromFace(impl_->face, font_size);

    impl_->ctx.setFillStyle(BLRgba32(color.r, color.g, color.b, 255 * 0.6));
    impl_->ctx.fillRect(pos.x, pos.y, text.size() * 10, 20);

    impl_->ctx.setFillStyle(BLRgba32(255, 255, 255, color.a));
    impl_->ctx.fillUtf8Text(BLPoint(pos.x + 5, pos.y + 15), font, text.c_str());

    return true;
}

bool Graphics::draw_circle(const Point2i &pos, const Scalar &color, float size) {
    impl_->ctx.setFillStyle(BLRgba32(color.r, color.g, color.b, color.a));
    impl_->ctx.fillCircle(pos.x, pos.y, size);

    return true;
}

bool Graphics::draw_line(const Point2i &s_pos, const Point2i &e_pos, const Scalar &color, float width) {
    impl_->ctx.setStrokeStyle(BLRgba32(color.r, color.g, color.b, color.a));
    impl_->ctx.setStrokeWidth(width);
    impl_->ctx.strokeLine(s_pos.x, s_pos.y, e_pos.x, e_pos.y);

    return true;
}

cv::Mat Graphics::crop_image(Rect2f rect) {
    cv::Mat cropped;
    impl_->bgra(cv::Rect2f{rect.x, rect.y, rect.width, rect.height}).copyTo(cropped);
    return cropped;
}

}// namespace graphics
}// namespace gddi