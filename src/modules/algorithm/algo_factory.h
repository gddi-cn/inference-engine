#ifndef __ALGO_FACTORY_H__
#define __ALGO_FACTORY_H__

#include "tsing_inference.h"

namespace gddi {
namespace algo {

static std::unique_ptr<AbstractAlgo> make_algo_impl() {
    return std::make_unique<TsingInference>();
}
}// namespace algo
}// namespace gddi

#endif