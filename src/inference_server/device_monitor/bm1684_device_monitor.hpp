#ifndef __BM1684_DEVICE_MONITOR_HPP__
#define __BM1684_DEVICE_MONITOR_HPP__

#include <cstdio>
#ifdef WITH_BM1684

#include "base_device_monitor.hpp"
#include "modcrypt.h"
#include <map>
#include <memory>
#include <vector>

#define USE_OPENCV 1
#define USE_FFMPEG 1
#include "bm_wrapper.hpp"

namespace monitor_wrapper {

class BmDeviceMonitor : public BaseDeviceMonitor {
public:
    virtual void init_handle() {
        bm_dev_getcount(&device_count_);
        for (int i = 0; i < device_count_; i++) {
            device_handle_[i] =
                std::shared_ptr<bm_handle_t>(new bm_handle_t, [](bm_handle_t *ptr) { bm_dev_free(*ptr); });
            if (bm_dev_request(device_handle_[i].get(), i) != 0) { printf("** failed to request device-id: %d\n", i); }
        }
    }

    virtual uint32_t get_device_count() { return device_count_; }

    virtual std::string get_device_compute_capability(unsigned int device_id) { return {}; }

    virtual std::pair<uint32_t, uint32_t> get_device_power_status(unsigned int device_id) {
        std::pair<uint32_t, uint32_t> power_status{0, 0};
        bm_get_board_max_power(*device_handle_[device_id], &power_status.second);
        bm_get_board_power(*device_handle_[device_id], &power_status.first);
        power_status.second *= 1000;
        power_status.first *= 1000;
        return power_status;
    }

    virtual bool get_device_memory_info(unsigned int device_id, MemoryInfo &info) {
        bm_heap_stat_byte_t pheap_byte;
        if (bm_get_gmem_heap_stat_byte_by_id(*device_handle_[device_id], &pheap_byte, 0) == BM_SUCCESS) {
            info.total = pheap_byte.mem_total;
            info.free = pheap_byte.mem_avail;
            info.used = pheap_byte.mem_used;
            return true;
        }

        return false;
    }

    virtual bool get_vpu_memory_info(unsigned int device_id, MemoryInfo &info) {
        bm_heap_stat_byte_t pheap_byte;
        if (bm_get_gmem_heap_stat_byte_by_id(*device_handle_[device_id], &pheap_byte, 1) == BM_SUCCESS) {
            info.total = pheap_byte.mem_total;
            info.free = pheap_byte.mem_avail;
            info.used = pheap_byte.mem_used;
            return true;
        }

        return false;
    }

    virtual std::string get_device_name(unsigned int device_id) {
        char buffer[128];
        if (bm_get_board_name(*device_handle_[device_id], buffer) == BM_SUCCESS) { return buffer; }
        return {};
    }

    virtual std::string get_driver_version(unsigned int device_id) {
        int version = 0;
        bm_get_driver_version(*device_handle_[device_id], &version);
        return std::to_string((version >> 16) & 0xFF) + '.' + std::to_string((version >> 8) & 0xFF) + '.'
            + std::to_string(version & 0xFF);
    }

    virtual std::string get_device_serial_number() {
        // WARNING: 模型解密 salt 不可改
        return getDeviceUUID("inference-engine");
    }

    virtual bool get_devide_utilization_rate(unsigned int device_id, UtilizationRate &dev_utilization) {
        bm_dev_stat_t stat;
        if (bm_get_stat(*device_handle_[device_id], &stat) == BM_SUCCESS) {
            dev_utilization.gpu = stat.tpu_util;
            return true;
        }
        return false;
    }

private:
    std::map<uint32_t, std::shared_ptr<bm_handle_t>> device_handle_;
};

std::unique_ptr<BaseDeviceMonitor> make_device_monitor() { return std::make_unique<BmDeviceMonitor>(); }

}// namespace monitor_wrapper

#endif

#endif// __BM1684_DEVICE_MONITOR_HPP__