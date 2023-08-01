//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_HTTP_SERVICE_V1_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_HTTP_SERVICE_V1_HPP_

#include "../node_manager/node_manager.hpp"
#include "./_inference_preferences.hpp"
#include "./_inference_task_json.hpp"
#include "./_inference_utils.hpp"
#include "./inference_server_basic.hpp"
#include "basic_logs.hpp"
#include "device_monitor/base_device_monitor.hpp"
#include "device_monitor/tx5368_device_monitor.hpp"
#include "modules/modbus/modbus_device.h"
#include "modules/server_send/server_send.hpp"
#include "modules/server_send/ws_notifier.hpp"
#include "version.h"
#include <chrono>
#include <common_basic/thread_dbg_utils.hpp>
#include <hv/HttpServer.h>
#include <hv/WebSocketServer.h>
#include <iterator>
#include <regex>
#include <thread>

class HttpServiceTimeUsed {
private:
    HttpResponse *resp_;
    std::chrono::high_resolution_clock::time_point time_start_;

public:
    HttpServiceTimeUsed() = delete;
    explicit HttpServiceTimeUsed(HttpResponse *resp)
        : resp_(resp), time_start_(std::chrono::high_resolution_clock::now()) {}
    ~HttpServiceTimeUsed() {
        auto time_end_ = std::chrono::high_resolution_clock::now();
        std::stringstream oss;
        oss << std::chrono::duration<double>(time_end_ - time_start_).count() * 1000 << " ms";
        resp_->headers["Time-Used"] = oss.str();
    }
};

class InferenceHttpService {
private:
    // preferences
    InferencePreferences &preferences_;
    // core basic
    InferenceServerBasic *server_basic_;
    // http api server
    HttpService router_;
    websocket_server_t server_;

    std::string serial_number_;
    std::unique_ptr<monitor_wrapper::BaseDeviceMonitor> moniter_;

    std::unique_ptr<gddi::ModbusDevice> modbus_device_;

public:
    InferenceHttpService() = delete;
    explicit InferenceHttpService(InferencePreferences &preferences, InferenceServerBasic *server_basic)
        : preferences_(preferences), server_basic_(server_basic) {
        moniter_ = monitor_wrapper::make_device_monitor();
        moniter_->init_handle();
        serial_number_ = moniter_->get_device_serial_number();
        modbus_device_ = std::make_unique<gddi::ModbusDevice>();
    }

    void listen() {
        // 1. register httplib service route
        setup_http_service();

        // 2. listen
        spdlog::info("Listen: http://{}:{}", preferences_.host, preferences_.http_port);
        server_.port = preferences_.http_port;
        server_.service = &router_;
        server_.ws = &gddi::server_send::instance().get_websocket_service();
        server_.worker_threads = 2;
        gddi::thread_utils::set_cur_thread_name(__FUNCTION__);
        http_server_run(&server_, 0);

        while (true) {
            monitor_wrapper::MemoryInfo info;
#if !defined(WITH_RV1126)
            // system memory
            if (moniter_->get_system_memory_info(info)) {
                auto using_rate = info.used / info.total;
                if (using_rate > 0.9) {
                    spdlog::warn("System memory usage: {}, exit", using_rate);
                    exit(1);
                }
            }
#endif
            // npu or gpu memory
            if (moniter_->get_device_memory_info(0, info)) {
                auto using_rate = info.used * 1.0 / info.total;
                if (using_rate > 0.9) {
                    spdlog::warn("Device memory usage: {}, exit", using_rate);
                    exit(1);
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    void setup_http_service() {
        const auto &api_ = preferences_.http_service_api_prefix;
        // server_.set_logger([](const auto &req, const auto &res) {
        //     if (req.method != "GET") {
        //         std::cout << "REQ: " << std::setw(6) << req.method << ", " << req.path << std::endl;
        //     }
        // });

        // router_.document_root = preferences_.debug_www_dir;
        router_.GET((api_ + "/task").c_str(), [=](auto &ctx) { return _task_get(ctx); });
        router_.POST((api_ + "/task").c_str(), [=](auto &ctx) { return _task_create(ctx); });
        router_.Delete((api_ + "/task").c_str(), [=](auto &ctx) { return _task_delete(ctx); });
        router_.POST((api_ + "/preview").c_str(), [=](auto &ctx) { return _task_preview(ctx); });
        router_.POST((api_ + "/preview_jpeg").c_str(), [=](auto &ctx) { return _task_preview_jpeg(ctx); });
        router_.Delete((api_ + "/preview_jpeg").c_str(), [=](auto &ctx) { return _task_preview_jpeg_close(ctx); });
        router_.GET((api_ + "/dev_info").c_str(), [=](auto &ctx) { return _device_info(ctx); });
        router_.GET((api_ + "/node_constraints").c_str(), [=](auto &ctx) { return _node_constraints(ctx); });
        router_.GET((api_ + "/node_constraints_v2").c_str(), [=](auto &ctx) { return _node_constraints_v2(ctx); });
        router_.GET((api_ + "/version").c_str(), [=](auto &ctx) {
            nlohmann::json ver_obj;
            ver_obj["version"] = PROJECT_VERSION;
            ver_obj["branch"] = GIT_BRANCH;
            ver_obj["hash"] = GIT_HASH;
            ver_obj["build_time"] = BUILD_TIME;
            return ctx->send(ver_obj.dump());
        });

#if defined(WITH_MODBUS)
        router_.GET((api_ + "/modbus/experiments").c_str(), [=](auto &ctx) { return _modbus_slave_list(ctx); });
        router_.POST((api_ + "/modbus/experiments").c_str(), [=](auto &ctx) { return _modbus_write_register(ctx); });
#endif

        router_.POST((api_ + "/feature").c_str(), [=](auto &ctx) { return _fature_generate(ctx); });
    }

    static int _node_constraints(const HttpContextPtr &ctx) {
        HttpServiceTimeUsed time_used(ctx->response.get());

        auto ack_json = gddi::NodeManager::get_instance().get_node_constraints();
        return ctx->send(ack_json.dump());
    }

    static int _node_constraints_v2(const HttpContextPtr &ctx) {
        HttpServiceTimeUsed time_used(ctx->response.get());

        auto ack_json = gddi::NodeManager::get_instance().get_node_constraints_v2();
        return ctx->send(ack_json.dump());
    }

    int _task_delete(const HttpContextPtr &ctx) {
        HttpServiceTimeUsed time_used(ctx->response.get());

        auto ack_json = nlohmann::json::object();

        auto iter = ctx->request->query_params.find("task_name");
        if (iter != ctx->request->query_params.end()) {
            auto result = server_basic_->delete_task(iter->second);
            ack_json["success"] = true;
        } else {
            std::stringstream oss;
            oss << "No Such Task Named: " << iter->second;
            ack_json["success"] = false;
            ack_json["error"] = oss.str();
        }
        ack_json["tasks"] = server_basic_->get_tasks_json();
        return ctx->send(ack_json.dump());
    }

    int _task_create(const HttpContextPtr &ctx) {
        HttpServiceTimeUsed time_used(ctx->response.get());

        auto ack_json = nlohmann::json::object();
        try {
            auto task_info = nlohmann::json::parse(ctx->body());
            auto result = server_basic_->create_task(task_info["name"].get<std::string>(),
                                                     task_info["config"].get<std::string>());
            ack_json["success"] = result.success;
            ack_json["error"] = result.result;
            if (result.success) { ack_json["tasks"] = server_basic_->get_tasks_json(); }
        } catch (std::exception &exception) {
            ack_json["success"] = false;
            ack_json["error"] = exception.what();
            spdlog::error("[CREATE] {}", exception.what());
        }

        ws_notifier::push_status("tasks_update", ack_json);
        return ctx->send(ack_json.dump());
    }

    int _task_get(const HttpContextPtr &ctx) {
        HttpServiceTimeUsed time_used(ctx->response.get());

        auto task_list = server_basic_->get_tasks_json();
        return ctx->send(task_list.dump());
    }

    int _task_preview(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::object();

        auto req_obj = nlohmann::json::parse(ctx->body());
        auto task_name = req_obj["task_name"].get<std::string>();
        auto stream_url = req_obj["stream_url"].get<std::string>();
        auto codec = req_obj["codec"].get<std::string>();
        auto node_id = req_obj["node_id"].get<int>();

        auto result = server_basic_->preview_task(task_name, codec, stream_url, node_id);
        ack_json["success"] = result.success;
        ack_json["error"] = result.result;

        ws_notifier::push_status("tasks_async");
        return ctx->send(ack_json.dump());
    }

    int _task_preview_jpeg(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::object();

        auto req_obj = nlohmann::json::parse(ctx->body());

        int scale = 1;
        if (req_obj.count("scale") > 0) { scale = req_obj["scale"].get<int>(); }
        auto result = server_basic_->task_preview_jpeg(req_obj["task_name"].get<std::string>(),
                                                       req_obj["node_name"].get<std::string>(), scale);
        ack_json["success"] = result.success;
        ack_json["error"] = result.result;

        ws_notifier::push_status("tasks_async");
        return ctx->send(ack_json.dump());
    }

    int _task_preview_jpeg_close(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::object();

        auto task_name_iter = ctx->request->query_params.find("task_name");
        auto node_name_iter = ctx->request->query_params.find("node_name");
        if (task_name_iter != ctx->request->query_params.end() && node_name_iter != ctx->request->query_params.end()) {
            auto result = server_basic_->task_preview_jpeg_close(task_name_iter->second, node_name_iter->second);
            ack_json["success"] = result.success;
            ack_json["error"] = result.result;
        } else {
            ack_json["success"] = false;
            ack_json["error"] = "Unrecognized parameter";
        }

        ws_notifier::push_status("tasks_async");
        return ctx->send(ack_json.dump());
    }

    int _device_info(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::array();
        auto dev_count = moniter_->get_device_count();
        for (uint32_t i = 0; i < dev_count; i++) {
            auto item = nlohmann::json::object();
            item["id"] = i;
            item["name"] = moniter_->get_device_name(i);
            item["version"] = moniter_->get_driver_version(i);
            item["serial_number"] = serial_number_;
            item["capability"] = moniter_->get_device_compute_capability(i);
            item["memory_total"] = 0;
            item["memory_free"] = 0;
            item["memory_used"] = 0;
            item["gpu_utilization_rate"] = 0;
            item["memory_utilization_rate"] = 0;

            auto power_status = moniter_->get_device_power_status(i);
            item["power_usage"] = power_status.first / 1000;
            item["power_limit"] = power_status.second / 1000;

            monitor_wrapper::MemoryInfo info;
            if (moniter_->get_device_memory_info(i, info)) {
                item["memory_total"] = info.total;
                item["memory_free"] = info.free;
                item["memory_used"] = info.used;
            }

            monitor_wrapper::UtilizationRate utilization;
            if (moniter_->get_devide_utilization_rate(i, utilization)) {
                item["gpu_utilization_rate"] = utilization.gpu;
                item["memory_utilization_rate"] = info.used * 100 / info.total;
            }

            ack_json.push_back(item);
        }

        return ctx->send(ack_json.dump());
    }

#if defined(WITH_MODBUS)
    int _modbus_slave_list(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::array();
        return ctx->send(modbus_device_->get_device_list().dump());
    }

    int _modbus_write_register(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::object();

        try {
            auto req_obj = nlohmann::json::parse(ctx->body());

            modbus_device_->init_connect(req_obj["address"].get<std::string>());
            modbus_device_->send_raw_request(req_obj["device_model"].get<std::string>(),
                                             req_obj["slave_id"].get<uint8_t>(), req_obj["properties"]);
            modbus_device_->close();

            ack_json["success"] = true;
            ack_json["message"] = "";
        } catch (const std::exception &e) {
            ack_json["success"] = false;
            ack_json["message"] = e.what();
            spdlog::error("modbus_write_register: {}", e.what());
        }

        return ctx->send(ack_json.dump());
    }
#endif

    int _fature_generate(const HttpContextPtr &ctx) {
        auto ack_json = nlohmann::json::object();

        auto req_obj = nlohmann::json::parse(ctx->body());
        auto app_id = req_obj["app_id"].get<std::string>();
        auto folder_path = req_obj["folder_path"].get<std::string>();

        std::string config;
        std::ifstream ifs("templates/feature-library.json");
        config.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();

        config = std::regex_replace(config, std::regex("\\$folder_path"), folder_path);
        config = std::regex_replace(config, std::regex("\\$model_id"), app_id);

        auto result = server_basic_->create_task(app_id, config);
        ack_json["success"] = result.success;
        ack_json["error"] = result.result;

        return ctx->send(ack_json.dump());
    }
};

#endif//FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_HTTP_SERVICE_V1_HPP_
