#ifndef __TRACK_SMOOTHING_H__
#define __TRACK_SMOOTHING_H__

#include "node_struct_def.h"
#include <memory>

namespace gddi {

class SmoothingPrivate;

enum class SmoothingType { kMedian, kKalman };

class TrackSmoothing {
public:
    TrackSmoothing(const SmoothingType type);
    ~TrackSmoothing();

    void smooth_keypoints(std::vector<nodes::PoseKeyPoint> &keypoints);

private:
    std::unique_ptr<SmoothingPrivate> up_impl_;
};

}// namespace gddi

#endif