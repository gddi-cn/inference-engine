#include "modbus_device.h"
#include "json.hpp"
#include <array>
#include <codecvt>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <locale>
#include <modbus.h>
#include <regex>
#include <thread>

template<class Facet>
struct deletable_facet : Facet {
    template<class... Args>
    explicit deletable_facet(Args &&...args) : Facet(std::forward<Args>(args)...) {}
    ~deletable_facet() override = default;
};

bool is_hex_number(const std::string &str) {
    static std::regex const re{R"(0[xX][0-9a-fA-F]+)"};
    return std::regex_match(str.data(), re);
}

namespace gddi {

static std::string utf8_2_gbk(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring tmp_wstr = converter.from_bytes(str);

    const char *GBK_LOCALE_NAME = "zh_CN.GBK";
    using codecvt = deletable_facet<std::codecvt_byname<wchar_t, char, mbstate_t>>;
    std::wstring_convert<codecvt> convert(new codecvt(GBK_LOCALE_NAME));
    return convert.to_bytes(tmp_wstr);
}

class ModbusPrivate {
public:
    explicit ModbusPrivate() {}

    void connect(const std::string &ip, const int port) {
        ctx_ = modbus_new_tcp(ip.c_str(), port);
        protocal_ = ModbusProtocol::TCP;

        if (protocal_ == ModbusProtocol::RTU) {
            modbus_set_slave(ctx_, 1);
            modbus_rtu_set_serial_mode(ctx_, MODBUS_RTU_RS485);
            modbus_rtu_set_rts(ctx_, MODBUS_RTU_RTS_UP);

            if (modbus_connect(ctx_) == -1) {
                throw std::runtime_error(std::string("Modbus connetion failed: ")
                                         + modbus_strerror(errno));
            }
        } else {
            if (modbus_connect(ctx_) == -1) {
                throw std::runtime_error(std::string("Modbus connetion failed: ")
                                         + modbus_strerror(errno));
            }
        }
    }

    void send(const uint8_t *data, const int dlen) {
        if (modbus_send_raw_request(ctx_, data, dlen * sizeof(uint8_t)) == -1) {
            throw std::runtime_error(std::string("Modbus write: ") + modbus_strerror(errno));
        }

        // uint8_t rsp[MODBUS_TCP_MAX_ADU_LENGTH];
        // if (modbus_receive_confirmation(ctx_, rsp) < 0) {
            // throw std::runtime_error(std::string("Modbus receive: ") + modbus_strerror(errno));
        // }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    void close() { modbus_close(ctx_); }

    ~ModbusPrivate() {
        if (ctx_) {
            modbus_close(ctx_);
            modbus_free(ctx_);
        }
    }

    modbus_t *ctx_;
    ModbusProtocol protocal_;
};

ModbusDevice::ModbusDevice() {
    impl_ = std::make_unique<ModbusPrivate>();
    nlohmann::json json_obj;
    std::ifstream file("config/modbus_cfg.json");
    if (file.is_open()) {
        std::string buffer((std::istream_iterator<char>(file)), std::istream_iterator<char>());
        json_obj = nlohmann::json::parse(buffer);
    }

    try {
        for (auto &item : json_obj.items()) {
            auto object = nlohmann::json::object();
            object["device_name"] = item.value()["name"];
            object["device_type"] = item.value()["type"];
            auto &properties = item.value()["properties"];
            for (auto &props : properties.items()) {
                auto type = properties[props.key()]["type"].get<std::string>();
                object["properties"][props.key()]["name"] = properties[props.key()]["name"];
                object["properties"][props.key()]["type"] = type;
                if (type == "number" || type == "string") {
                    object["properties"][props.key()]["value"] = props.value()["value"];
                } else if (type == "numberArray") {
                    object["properties"][props.key()]["enums"] = props.value()["enums"];
                } else if (type == "stringMap") {
                    object["properties"][props.key()]["options"] =
                        properties[props.key()]["options"];
                }

                if (properties[props.key()].count("default") > 0) {
                    object["properties"][props.key()]["default"] =
                        properties[props.key()]["default"];
                }
            }
            dev_list_[item.key()] = std::move(object);

            for (auto &value : item.value()["template"]) {
                template_list_[item.key()].push_back(value);
            }
        }
    } catch (const std::exception &e) { spdlog::error("Failed to parse modbus_cfg: {}", e.what()); }
}

ModbusDevice::~ModbusDevice() {}

void ModbusDevice::init_connect(const std::string &address) {
    if (address.substr(0, 4) == "/dev") {
        impl_->ctx_ = modbus_new_rtu(address.c_str(), 9600, 'N', 8, 1);
        impl_->protocal_ = ModbusProtocol::RTU;
    } else {
        int port = 80;
        std::string ip = address;

        int pos = address.find_first_of(':', 0);
        if (pos != address.npos) {
            port = atoi(address.substr(pos + 1).c_str());
            ip = address.substr(0, pos);
        }

        impl_->connect(ip, port);
        spdlog::info("modbus connect address: {}, port: {} ", ip, port);
    }
}

void ModbusDevice::send_raw_request(const std::string &device_model, const uint8_t slave_id,
                                    const nlohmann::json &properties) {
    if (template_list_.count(device_model) == 0) {
        throw std::runtime_error("No find modbus device");
    }

    std::regex pattern(R"(\{(.+?)\})");
    for (auto &item : template_list_[device_model]) {
        auto cmd_template = item.get<std::string>();

        // 遍历参数
        size_t index = 1;
        uint8_t values[512] = {slave_id};
        auto regex_begin = std::sregex_iterator(cmd_template.begin(), cmd_template.end(), pattern);
        for (auto iter = regex_begin; iter != std::sregex_iterator(); ++iter) {
            std::string match_str = (*iter)[1].str();
            if (is_hex_number(match_str)) {
                values[index++] = static_cast<uint8_t>(std::stoi(match_str, 0, 16) & 0xFF);
            } else {
                std::string value_type;
                nlohmann::json value_obj;
                if (properties.count(match_str) > 0) {
                    value_obj = properties[match_str];
                } else if (dev_list_[device_model]["properties"][match_str].count("default") > 0) {
                    value_obj = dev_list_[device_model]["properties"][match_str]["default"];
                }

                if (value_obj.is_null()) {
                    throw std::runtime_error(std::string("null properties: ") + match_str);
                }

                auto pro_type =
                    dev_list_[device_model]["properties"][match_str]["type"].get<std::string>();
                if (pro_type == "number") {
                    uint16_t value = value_obj.get<uint16_t>();
                    values[index++] = (value & 0xFF00) >> 8;
                    values[index++] = value & 0x00FF;
                } else if (pro_type == "string") {
                    auto gbk_str = utf8_2_gbk(value_obj.get<std::string>());
                    memcpy(&values[index], gbk_str.data(), gbk_str.size());
                    index += gbk_str.size();
                } else if (pro_type == "stringMap") {
                    if (value_obj.get<std::string>().empty()) {
                        value_obj = dev_list_[device_model]["properties"][match_str]["default"];
                    }
                    uint16_t value = std::stoi(value_obj.get<std::string>(), 0, 16);
                    values[index++] = (value & 0xFF00) >> 8;
                    values[index++] = value & 0x00FF;
                }
            }
        }

        impl_->send(values, index);
    }
}

void ModbusDevice::close() {
    impl_->close();
    spdlog::info("modbus close");
}

const nlohmann::json ModbusDevice::get_device_list() {
    nlohmann::json devices;
    for (const auto &item : dev_list_.items()) {
        nlohmann::json device = item.value();
        device["device_model"] = item.key();
        devices.emplace_back(std::move(device));
    }
    return devices;
}

}// namespace gddi