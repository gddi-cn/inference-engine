#ifndef __TX5368_DEVICE_MONITOR_HPP__
#define __TX5368_DEVICE_MONITOR_HPP__

#include "api/global_config.h"
#include "api/infer_api.h"
#include "base_device_monitor.hpp"
#include "core/device.h"
#include <memory>

namespace monitor_wrapper {

class TsDeviceMonitor : public BaseDeviceMonitor {
public:
    TsDeviceMonitor() { gddeploy::gddeploy_init(""); }
    virtual ~TsDeviceMonitor() {}

    virtual uint32_t get_device_count() override { return 1; }

    virtual std::string get_device_serial_number() override {
        return gddeploy::DeviceManager::Instance()->GetDevice()->GetDeviceUUID();
    }
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() { return std::make_unique<TsDeviceMonitor>(); }

}// namespace monitor_wrapper

#endif// __TX5368_DEVICE_MONITOR_HPP__