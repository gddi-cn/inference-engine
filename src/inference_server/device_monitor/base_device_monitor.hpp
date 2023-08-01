//
// Created by cc on 2022/2/10.
//

#ifndef INFERENCE_ENGINE_SRC__INFERENCE_SERVER__DEVICE_MONITOR_DEVICE_MONITOR_HPP_
#define INFERENCE_ENGINE_SRC__INFERENCE_SERVER__DEVICE_MONITOR_DEVICE_MONITOR_HPP_

#include <memory>
#include <stdint.h>
#include <string>
#include <sys/sysinfo.h>
#include <utility>

namespace monitor_wrapper {

struct MemoryInfo {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

struct UtilizationRate {
    unsigned int gpu;
    unsigned int memory;
};

class BaseDeviceMonitor {
public:
    BaseDeviceMonitor() {}
    virtual ~BaseDeviceMonitor() {}

    virtual void init_handle() {}

    virtual uint32_t get_device_count() { return device_count_; }

    virtual std::string get_device_compute_capability(unsigned int device_id) { return {}; }

    virtual std::pair<uint32_t, uint32_t> get_device_power_status(uint32_t device_id) { return {1, 1}; }

    virtual bool get_device_memory_info(unsigned int device_id, MemoryInfo &info) { return false; }

    virtual bool get_vpu_memory_info(unsigned int device_id, MemoryInfo &info) { return false; }

    virtual std::string get_device_name(unsigned int device_id) { return {}; }

    virtual std::string get_driver_version(unsigned int device_id) { return {}; }

    virtual std::string get_device_serial_number() { return "null-device"; }

    virtual bool get_devide_utilization_rate(unsigned int device_id, UtilizationRate &dev_utilization) { return false; }

    virtual bool get_system_memory_info(MemoryInfo &info) {
        struct sysinfo s_info;
        if (sysinfo(&s_info) == 0) {
            info.total = s_info.totalram;
            info.free = s_info.freeram;
            info.used = s_info.totalram - s_info.freeram;
            return true;
        }
        return false;
    }

protected:
#ifdef WITH_BM1684
    int device_count_{0};
#else
    uint32_t device_count_{0};
#endif
};

}// namespace monitor_wrapper

#endif//INFERENCE_ENGINE_SRC__INFERENCE_SERVER__DEVICE_MONITOR_DEVICE_MONITOR_HPP_
