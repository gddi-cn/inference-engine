#include "geometry.h"

namespace gddi {
namespace geometry {

polygon_type convert_polygon(const std::vector<Point2i> &region) {
    polygon_type poly;
    for (auto &point : region) { bg::append(poly, bg::make<bg::model::d2::point_xy<float>>(point.x, point.y)); }
    bg::correct(poly);
    return poly;
}

float intersection(const polygon_type &poly1, const polygon_type &poly2) {
    std::vector<polygon_type> inter_output;
    bg::intersection(poly1, poly2, inter_output);
    float inter_area = 0;
    for (auto &p : inter_output) { inter_area += bg::area(p); }
    return inter_area;
}

float union_(const polygon_type &poly1, const polygon_type &poly2) {
    std::vector<polygon_type> union_output;
    bg::union_(poly1, poly2, union_output);
    float union_area = 0;
    for (auto &p : union_output) { union_area += bg::area(p); }
    return union_area;
}

bool has_intersection(const std::vector<Point2i> &region1, const std::vector<Point2i> &region2) {
    polygon_type poly1 = convert_polygon(region1);
    polygon_type poly2 = convert_polygon(region2);
    return intersection(poly1, poly2) > 0;
}

std::vector<Point2i> merge_region(const std::vector<Point2i> &inter_region,
                                  const std::map<std::string, std::vector<Point2i>> &all_regions) {
    std::vector<Point2i> vec_merge_region;

    polygon_type poly1 = convert_polygon(inter_region);
    for (auto &[key, points] : all_regions) {
        polygon_type poly2 = convert_polygon(points);
        std::vector<polygon_type> union_output;
        bg::union_(poly1, poly2, union_output);
        if (!union_output.empty()) { poly1 = union_output[0]; }
    }

    for (auto &point : poly1.outer()) { vec_merge_region.push_back({(int)point.x(), (int)point.y()}); }

    return vec_merge_region;
}

std::vector<Point2i> merge_region(const std::vector<Point2i> &inter_region, const std::vector<Point2i> &region2) {
    std::vector<Point2i> vec_merge_region;

    polygon_type poly1 = convert_polygon(inter_region);
    polygon_type poly2 = convert_polygon(region2);
    std::vector<polygon_type> union_output;
    bg::union_(poly1, poly2, union_output);
    if (!union_output.empty()) { poly1 = union_output[0]; }

    for (auto &point : poly1.outer()) { vec_merge_region.push_back({(int)point.x(), (int)point.y()}); }

    return vec_merge_region;
}

float polygon_area(const std::vector<Point2i> &region) { return bg::area(convert_polygon(region)); }

float area_cover_rate(const std::vector<Point2i> &region1, const std::vector<Point2i> &region2) {
    polygon_type poly1 = convert_polygon(region1);
    polygon_type poly2 = convert_polygon(region2);

    auto inter_area = intersection(poly1, poly2);

    return inter_area / std::min(bg::area(poly1), bg::area(poly2));
}

bool point_within_area(const Point2i &point, const std::vector<Point2i> &region) {
    polygon_type poly = convert_polygon(region);
    return boost::geometry::within(point_type(point.x, point.y), poly);
}

}// namespace geometry
}// namespace gddi