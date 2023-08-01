#ifndef __NVIDIA_DEVICE_MONITOR_HPP__
#define __NVIDIA_DEVICE_MONITOR_HPP__

#if defined(WITH_NVIDIA)

#include "alg.h"
#include "base_device_monitor.hpp"
#include <memory>
#include <nvml.h>
#include <vector>

namespace monitor_wrapper {

class NvDeviceMonitor : public BaseDeviceMonitor {
public:
    NvDeviceMonitor() {}
    virtual ~NvDeviceMonitor() { nvmlShutdown(); }

    virtual void init_handle() {
        try {
            err_code_ = nvmlInit_v2();
            err_code_ = nvmlDeviceGetCount_v2(&device_count_);

            for (int i = 0; i < device_count_; i++) {
                auto handle = std::make_unique<nvmlDevice_t>();
                if (nvmlDeviceGetHandleByIndex_v2(i, handle.get()) == NVML_SUCCESS) {
                    device_handle_[i] = std::move(handle);
                } else {
                    fprintf(stderr, "%s", nvmlErrorString(err_code_));
                }
            }
        } catch (const std::exception &e) { printf("%s\n", e.what()); }
    }

    virtual uint32_t get_device_count() { return device_count_; }

    virtual std::string get_device_compute_capability(unsigned int device_id) {
        int major = 0, minor = 0;
        if (nvmlDeviceGetCudaComputeCapability(*device_handle_[device_id], &major, &minor)
            == NVML_SUCCESS) {
            auto capability = std::to_string(major * 1.0 + minor * 0.1);
            auto pos = capability.find('.');
            if (pos != capability.npos) { capability = capability.substr(0, pos + 3); }
            return capability;
        }
        return {};
    }

    virtual std::pair<uint32_t, uint32_t> get_device_power_status(unsigned int device_id) {
        std::pair<uint32_t, uint32_t> power_status;
        nvmlDeviceGetEnforcedPowerLimit(*device_handle_[device_id], &power_status.second);
        nvmlDeviceGetPowerUsage(*device_handle_[device_id], &power_status.first);
        return power_status;
    }

    virtual bool get_device_memory_info(unsigned int device_id, MemoryInfo &info) {
        if (device_id > device_handle_.size()) { return false; }

        nvmlBAR1Memory_t memory;
        if (nvmlDeviceGetBAR1MemoryInfo(*device_handle_[device_id], &memory) == NVML_SUCCESS) {
            info = {memory.bar1Total, memory.bar1Free, memory.bar1Used};
            return true;
        }

        return false;
    }

    virtual std::string get_device_name(unsigned int device_id) {
        char buffer[NVML_DEVICE_NAME_BUFFER_SIZE];
        if (nvmlDeviceGetName(*device_handle_[device_id], buffer, NVML_DEVICE_NAME_BUFFER_SIZE)
            == NVML_SUCCESS) {
            return buffer;
        }
        return {};
    }

    virtual std::string get_driver_version(unsigned int device_id) {
        char buffer[NVML_DEVICE_VBIOS_VERSION_BUFFER_SIZE];
        if (nvmlDeviceGetVbiosVersion(*device_handle_[device_id], buffer,
                                      NVML_DEVICE_VBIOS_VERSION_BUFFER_SIZE)
            == NVML_SUCCESS) {
            return buffer;
        }
        return {};
    }

    virtual std::string get_device_serial_number() { return AlgImpl::GetDeviceSerialNumber(); }

    virtual bool get_devide_utilization_rate(unsigned int device_id,
                                             UtilizationRate &dev_utilization) {
        nvmlUtilization_t nv_utilization;
        if (nvmlDeviceGetUtilizationRates(*device_handle_[device_id], &nv_utilization)
            == NVML_SUCCESS) {
            dev_utilization = {nv_utilization.gpu, nv_utilization.memory};

            return true;
        }
        return false;
    }

private:
    std::map<uint32_t, std::unique_ptr<nvmlDevice_t>> device_handle_;
    nvmlReturn_t err_code_;
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() {
    return std::make_unique<NvDeviceMonitor>();
}

}// namespace monitor_wrapper

#endif

#endif// __NVIDIA_DEVICE_MONITOR_HPP__