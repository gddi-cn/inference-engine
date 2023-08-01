#ifndef __RV1126_DEVICE_MONITOR_HPP__
#define __RV1126_DEVICE_MONITOR_HPP__

#if defined(WITH_RV1126)

#include "alg.h"
#include "base_device_monitor.hpp"
#include <memory>

namespace monitor_wrapper {

class RvDeviceMonitor : public BaseDeviceMonitor {
public:
    RvDeviceMonitor() {}
    virtual ~RvDeviceMonitor() {}

    virtual uint32_t get_device_count() override { return 1; }

    virtual std::string get_device_serial_number() override { return AlgImpl::GetDeviceSerialNumber(); }
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() { return std::make_unique<RvDeviceMonitor>(); }

}// namespace monitor_wrapper

#endif

#endif// __RV1126_DEVICE_MONITOR_HPP__