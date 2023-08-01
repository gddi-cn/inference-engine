//
// Created by cc on 2021/11/22.
//

#include "inference_server_v0.hpp"
#include "./_inference_http_service_v1.hpp"
#include "./_inference_preferences.hpp"
#include "./_inference_task.hpp"
#include "./_inference_types.hpp"
#include "./inference_server_basic.hpp"
#include "basic_logs.hpp"
#include "runnable_node.hpp"
#include <atomic>
#include <json.hpp>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

class gddi::InferenceServer_v0::InferenceServerImpl : public InferenceServerBasic {
private:
    // preferences
    InferencePreferences preferences_;

    // http api server
    std::unique_ptr<InferenceHttpService> http_service_;

    // all running task container
    std::map<std::string, std::shared_ptr<InferenceTask>> tasks_;

    // all preview nodes
    std::map<std::string, int> preview_;

    // public API thread-safe lock
    std::mutex mutex_;

public:
    InferenceServerImpl() = default;

    void launch(const std::string &host, uint32_t port) {
        preferences_.host = host;
        preferences_.http_port = port;

        print_server_status_(std::cout);
        http_service_ = std::make_unique<InferenceHttpService>(preferences_, this);
        http_service_->listen();
    }

    ResultMsg create_task(const std::string &task_name, const std::string &text) override {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        InferenceSliceConfig task_json;
        auto result = InferenceSliceConfig::parse(text, task_json);
        if (result.success) {
            if (tasks_.count(task_name) > 0) { return {false, std::string("task ") + task_name + " already exist!"}; }

            auto result_msg = create_v0_(task_name, task_json);
            auto task_name_prefix = task_name.substr(0, 36);
            if (result_msg.success && preview_.count(task_name_prefix) > 0) {
                nlohmann::json json_args;
                json_args["$scale"] = preview_[task_name_prefix];
                tasks_[task_name]->preview_node(InferenceTask::PreviewType::kJpeg_v1, json_args);
            }
            return result_msg;
        }
        return result;
    }

    ResultMsg delete_task(const std::string &task_name) override {
        using namespace std::chrono;

        auto start = std::chrono::steady_clock::now();

        // safely remove task from task list
        auto task = take_task_(task_name);

        auto task_exist = task != nullptr;

        // delete hold ptr, trigger the destructor
        task.reset();

        auto delete_time_used_ = duration_cast<milliseconds>(steady_clock::now() - start).count();
        spdlog::info("[DELETE] task: {}, exist: {}, time: {}ms", task_name, task_exist, delete_time_used_);

        return {task_exist};
    }

    std::ostream &print_server_status_(std::ostream &oss) {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        // cache flags
        OStreamFlagsHolder flags_holder(oss);

        // print total tasks
        if (!tasks_.empty()) {
            spdlog::info("++++++++++++++++++++++++++++ Task List ++++++++++++++++++++++++++++");
            spdlog::info("#Name\t\t\t\t\t\t#Status\t\t#Code");
            for (auto &item : tasks_) {
                if (item.second->is_running()) {
                    spdlog::info("{}\t{}\t\t  {}", item.first, "running", 0);
                } else {
                    spdlog::info("{}\t{}\t\t  {}", item.first, "stop", item.second->quit_code());
                }
            }
            spdlog::info("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        }

        // int task_id = 0;
        // for (const auto &task : tasks_) {
        //     oss << std::setw(6) << task_id++ << ") ";
        //     task.second->_dbg_print_task_status(oss, 8);
        // }
        return oss;
    }

    nlohmann::json get_tasks_json() override {
        std::lock_guard<std::mutex> lock_guard(mutex_);
        auto task_list = nlohmann::json::array();
        for (const auto &task : tasks_) { task_list.push_back(task.second->get_json_detail()); }
        return task_list;
    }

    ResultMsg preview_task(const std::string &task_name, const std::string &codec, const std::string &stream_url,
                           int node_id) override {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        if (tasks_.count(task_name) > 0) {
            // release precious preview
            for (auto &task : tasks_) { task.second->release_preview(); }

            // launch new preview
            nlohmann::json json_args;
            json_args["$codec"] = codec;
            json_args["$stream_url"] = stream_url;
            json_args["preview_node_id"] = node_id;
            return tasks_[task_name]->preview_node(InferenceTask::PreviewType::kLiveStreaming_v1, json_args);
        }
        return {false, "Unable to preview the task"};
    }

    ResultMsg task_preview_jpeg(const std::string &task_name, const std::string &node_name, const int scale) override {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        // 兼容轮询任务，预览传参 task_name 不带 _xx
        auto iter = std::find_if(tasks_.begin(), tasks_.end(),
                                 [&task_name](const std::pair<std::string, std::shared_ptr<InferenceTask>> &task) {
                                     return strstr(task.first.c_str(), task_name.c_str()) != nullptr;
                                 });

        if (iter != tasks_.end()) {
            if (preview_.count(task_name) > 0) { return {true, ""}; }

            // release precious preview
            // tasks_[iter->first]->release_preview();

            // launch new preview
            nlohmann::json json_args;

            json_args["$node_name"] = node_name;
            json_args["$scale"] = scale;
            auto result_msg = tasks_[iter->first]->preview_node(InferenceTask::PreviewType::kJpeg_v1, json_args);
            if (result_msg.success) {
                preview_[task_name] = scale;
                spdlog::info("=================== Task: {}, Node: {} preview stared!", task_name, node_name);
            } else {
                spdlog::info("=================== Task: {}, Node: {}, preview failed!", task_name, node_name);
            }
            return result_msg;
        }
        return {false, "This task is not exist"};
    }

    ResultMsg task_preview_jpeg_close(const std::string &task_name, const std::string &node_name) override {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        for (auto &task : tasks_) {
            if (task.first.substr(0, 36) == task_name) {
                task.second->release_preview();
                preview_.erase(task_name);
            }
        }

        spdlog::info("=================== Task: {}, Node: {}, preview stop!", task_name, node_name);

        return {true, "success"};
    }

    std::shared_ptr<InferenceTask> take_task_(const std::string &task_name) {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        auto iter = tasks_.find(task_name);
        if (iter != tasks_.end()) {
            auto cur_task = iter->second;
            tasks_.erase(iter);
            return cur_task;
        }
        return std::shared_ptr<InferenceTask>();
    }

    /**
     * @brief 这个函数被设计为总是被异步调用，也就是从其它线程来访问。
     *
     *        不可以在 Runner 的消息循环线程中运行，会有不可预知的问题发生！！！
     * @param code
     * @param task_name
     */
    void on_task_quit_(int code, const std::string &task_name) {
        std::stringstream oss;
        spdlog::info("### task: {}, quit with {}", task_name, code);

        // clean task resources, only trigger by exception quit
        {
            std::lock_guard<std::mutex> lock_guard(mutex_);
            auto iter = tasks_.find(task_name);
            if (iter != tasks_.end()) {
                iter->second->_dbg_print_task_status(oss, 4, true);

                // 1. release all nodes
                // 2. release all runners
                iter->second->release_runners_nodes();
            }
        }

        std::cout << oss.str() << std::endl;
        print_server_status_(std::cout);

        ws_notifier::push_status("task_quit");
    }

    ResultMsg create_v0_(const std::string &task_name, const InferenceSliceConfig &task_json) {
        // Create task context
        auto task = InferenceTask::create_from(task_name, task_json);

        // Finally hold the task
        tasks_[task_name] = task;

        // Launch the task
        task->launch([=](int code) { on_task_quit_(code, task_name); });

        spdlog::info("### Create task: {}", task_name);

        return {true};
    }
};

gddi::InferenceServer_v0::InferenceServer_v0() : impl_(new InferenceServerImpl) {}

gddi::InferenceServer_v0::~InferenceServer_v0() = default;

void gddi::InferenceServer_v0::launch(const std::string &host, uint32_t port) { impl_->launch(host, port); }

bool gddi::InferenceServer_v0::run_v0_task(const std::string &task_name, const std::string &json_text) {
    return impl_->create_task(task_name, json_text).success;
}