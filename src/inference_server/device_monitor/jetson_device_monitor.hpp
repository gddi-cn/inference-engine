#ifndef __JETSON_DEVICE_MONITOR_HPP__
#define __JETSON_DEVICE_MONITOR_HPP__

#if defined(WITH_JETSON)

#include "alg.h"
#include "base_device_monitor.hpp"
#include <memory>
#include <vector>

namespace monitor_wrapper {

class JetsonDeviceMonitor : public BaseDeviceMonitor {
public:
    JetsonDeviceMonitor() {}
    virtual ~JetsonDeviceMonitor() {}

    virtual uint32_t get_device_count() {
        return 1;
    }

    virtual std::string get_device_name(unsigned int device_id) {
        return "Xavier NX";
    }

    virtual std::string get_device_serial_number() { return AlgImpl::GetDeviceSerialNumber(); }
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() {
    return std::make_unique<JetsonDeviceMonitor>();
}

}// namespace monitor_wrapper

#endif

#endif// __JETSON_DEVICE_MONITOR_HPP__