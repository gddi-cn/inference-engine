#ifndef __INTEL_DEVICE_MONITOR_HPP__
#define __INTEL_DEVICE_MONITOR_HPP__

#ifdef WITH_INTEL

#include "base_device_monitor.hpp"
#include "gddi_api.h"
#include <memory>

namespace monitor_wrapper {

class IntelDeviceMonitor : public BaseDeviceMonitor {
public:
    IntelDeviceMonitor() {}
    virtual ~IntelDeviceMonitor() {}

    virtual uint32_t get_device_count() override { return 1; }

    virtual std::string get_device_serial_number() override { return gdd::GetDeviceUUID(); }
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() {
    return std::make_unique<IntelDeviceMonitor>();
}

}// namespace monitor_wrapper

#endif

#endif// __INTEL_DEVICE_MONITOR_HPP__