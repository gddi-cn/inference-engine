/**
 * @file modbus_device.hpp
 * @author zhdotcai
 * @brief 485通讯协议封装
 * @version 0.1
 * @date 2022-07-20
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __MODBUS_ALARM_H__
#define __MODBUS_ALARM_H__

#include "basic_logs.hpp"
#include "json.hpp"
#include <string>
#include <vector>

namespace gddi {

class ModbusPrivate;
enum class ModbusProtocol { RTU = 0, TCP = 1 };

class ModbusDevice {
public:
    ModbusDevice();
    virtual ~ModbusDevice();
    virtual void init_connect(const std::string &address);
    virtual void send_raw_request(const std::string &device_model, const uint8_t slave_id,
                                  const nlohmann::json &properties);
    virtual void close();

    const nlohmann::json get_device_list();

private:
    nlohmann::json dev_list_;
    nlohmann::json template_list_;
    std::unique_ptr<ModbusPrivate> impl_;
};

}// namespace gddi

#endif