#ifndef __MLU2XX_DEVICE_MONITOR_HPP__
#define __MLU2XX_DEVICE_MONITOR_HPP__

#if defined(WITH_MLU220) || defined(WITH_MLU270) || defined(WITH_MLU370)

#include "base_device_monitor.hpp"
#include "gdd_api.h"
#include "gdd_device.h"
#include <memory>

namespace monitor_wrapper {

class MluDeviceMonitor : public BaseDeviceMonitor {
public:
    MluDeviceMonitor() {}
    virtual ~MluDeviceMonitor() {}

    virtual uint32_t get_device_count() override { return 1; }

    virtual std::string get_device_name(unsigned int device_id) override { return device_.get_device_name(device_id); }

    virtual std::string get_driver_version(unsigned int device_id) override {
#if defined(WITH_MLU220)
        return device_.get_driver_version(device_id);
#else
        return "";
#endif
    }

    virtual std::string get_device_serial_number() override { return gdd::GetDeviceUUID(); }

private:
    gdd::Device device_;
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() { return std::make_unique<MluDeviceMonitor>(); }

}// namespace monitor_wrapper

#endif

#endif// __MLU2XX_DEVICE_MONITOR_HPP__