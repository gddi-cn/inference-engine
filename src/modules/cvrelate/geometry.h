#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

#include "types.hpp"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <vector>

namespace bg = boost::geometry;

namespace gddi {
namespace geometry {

using point_type = bg::model::d2::point_xy<float>;
using polygon_type = bg::model::polygon<point_type>;

polygon_type convert_polygon(const std::vector<Point2i> &region);

float intersection(const polygon_type &poly1, const polygon_type &poly2);

float union_(const polygon_type &poly1, const polygon_type &poly2);

bool has_intersection(const std::vector<Point2i> &region1, const std::vector<Point2i> &region2);

std::vector<Point2i> merge_region(const std::vector<Point2i> &inter_region,
                                  const std::map<std::string, std::vector<Point2i>> &all_regions);

std::vector<Point2i> merge_region(const std::vector<Point2i> &inter_region, const std::vector<Point2i> &region2);

float polygon_area(const std::vector<Point2i> &region);

float area_cover_rate(const std::vector<Point2i> &region1, const std::vector<Point2i> &region2);

bool point_within_area(const Point2i &point, const std::vector<Point2i> &region);

}// namespace geometry
}// namespace gddi

#endif