//
// Created by cc on 2021/11/29.
//

#ifndef FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TASK_HPP_
#define FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TASK_HPP_
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <map>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <modules/server_send/ws_notifier.hpp>
#include "runnable_node.hpp"
#include "utils.hpp"
#include "helper/inference_slice_v1.hpp"

class InferenceTask : public std::enable_shared_from_this<InferenceTask> {
public:
    enum class PreviewType {
        kNone,
        kJpeg_v1,
        kLiveStreaming_v1,
    };

private:
    int preview_quit_code_{0};
    PreviewType preview_type_{PreviewType::kNone};
    const std::string name_;                            // 任务名
    std::shared_ptr<inference_slice> inference_slice_;  // 主要的逻辑推理
    std::shared_ptr<inference_slice> preview_slice_;    // 预览专用片段

public:
    InferenceTask() = delete;
    explicit InferenceTask(std::string name) : name_(std::move(name)) {}

    static std::shared_ptr<InferenceTask> create_from(const std::string &task_name,
                                                      const InferenceSliceConfig &task_json) {
        auto task = std::make_shared<InferenceTask>(task_name);
        task->inference_slice_ = inference_slice::create_from(task_name, task_json);
        return task;
    }

    std::shared_ptr<gddi::ngraph::NodeAny> find_first_node_by_type(const std::string &n_type) {
        return inference_slice_->find_first_node_by_type(n_type);
    }

    ResultMsg preview_(PreviewType preview_type,
                       const std::shared_ptr<gddi::ngraph::NodeAny> &preview_node_,
                       const std::string &slice_path, const nlohmann::json &args) {
        if (preview_node_) {
            auto weak_this_ = weak_from_this();
            preview_slice_ = attach_slice_for(preview_node_, slice_path, args);
            preview_slice_->run_slice([=](int code) {
                // TODO: notify client, preview stopped
                spdlog::info("=================== Task: {}, preview stopped! {}", name_, code);
                ws_notifier::push_status("tasks_async");

                auto this_ = weak_this_.lock();
                if (this_) {
                    this_->preview_type_ = PreviewType::kNone;
                    this_->preview_quit_code_ = code;
                }
            });
            preview_type_ = preview_type;
            return {true, "success"};
        }
        throw std::runtime_error(std::string("Can not find preview node"));
    }

    ResultMsg preview_node(PreviewType preview_type, const nlohmann::json &args) {
        try {
            switch (preview_type) {
                case PreviewType::kLiveStreaming_v1: {
                    auto node_id_ = args["preview_node_id"].get<int>();
                    auto preview_node_ = inference_slice_->find_node_by_id(node_id_);
                    return preview_(preview_type, preview_node_, "live-preview.json", args);
                }
                case PreviewType::kJpeg_v1: {
                    auto preview_node = inference_slice_->find_first_node_by_type("JpegPreviewer_v2");
                    if (preview_node) {
                        preview_node->set_spec_data(
                            {{"enable", "true"}, {"scale", std::to_string(args["$scale"].get<int>())}});
                    } else {
                        preview_node = inference_slice_->find_last_node_of_preview();
                        preview_node->set_spec_data({{"enable", "true"}});
                        preview_(preview_type, preview_node, "jpeg-preview.json", args);
                        preview_slice_->find_first_node_by_type("JpegPreviewer_v2")
                            ->set_spec_data({{"enable", "true"}});
                    }
                    return {true, "success"};
                }
                case PreviewType::kNone: {
                    preview_slice_.reset();
                    preview_type_ = preview_type;
                }
                default:
                    throw std::runtime_error("Un-supported preview type");
            }
        } catch (std::exception &ex) { return {false, ex.what()}; }
    }

    nlohmann::json get_json_detail() const {
        const auto &runners = inference_slice_->runners();
        const auto &nodes = inference_slice_->nodes();

        auto task_json = nlohmann::json();
        task_json["name"] = name_;
        task_json["raw_config"] = inference_slice_->raw_json();
        task_json["runners"] = nlohmann::json::array();
        task_json["nodes"] = nlohmann::json::array();
        for (const auto &r : runners) { task_json["runners"].push_back(r.first); }
        for (const auto &n : nodes) {
            auto node_json = nlohmann::json();
            node_json["id"] = n.first;
            node_json["runner"] = n.second->get_runner()->name();
            node_json["ports_in"] = nlohmann::json::array();
            int i;
            for (i = 0;; i++) {
                auto result = n.second->get_endpoint_features(gddi::ngraph::endpoint::Type::Input, i);
                if (result.success) {
                    node_json["ports_in"].push_back(result.result.data_features);
                } else {
                    break;
                }
            }

            node_json["ports_out"] = nlohmann::json::array();
            for (i = 0;; i++) {
                auto result = n.second->get_endpoint_features(gddi::ngraph::endpoint::Type::Output, i);
                if (result.success) {
                    node_json["ports_out"].push_back(result.result.data_features);
                } else {
                    break;
                }
            }

            auto logs = n.second->get_logs();
            node_json["running"] = logs.first;
            for (auto &item : logs.second) {
                auto item_obj = nlohmann::json::object();
                item_obj["time"] = item.first;
                item_obj["message"] = item.second;
                node_json["logs"].push_back(item_obj);
            }

            task_json["nodes"].push_back(node_json);
        }
        task_json["running"] = is_running();
        task_json["quit_code"] = quit_code();
        task_json["runtime_json"] = get_runtime_json();
        task_json["preview_status"] = (int)preview_type_;
        task_json["preview_quit_code"] = preview_quit_code_;
        return task_json;
    }

    void release_preview() {
        auto node = inference_slice_->find_first_node_by_type("JpegPreviewer_v2");
        if (node) {
            node->set_spec_data({});
        } else {
            preview_slice_.reset();
        }
    }
    void release_runners_nodes() { inference_slice_->release_runners_nodes(); }
    void launch(const std::function<void(int)> &on_task_quit) {
        inference_slice_->run_slice(on_task_quit);
    }
    bool is_running() const { return inference_slice_->is_running(); }
    int quit_code() const { return inference_slice_->quit_code(); }

    std::ostream &_dbg_print_task_status(std::ostream &oss, int tab_space, bool tab_first = false) {
        OStreamFlagsHolder flags_holder(oss);

        auto quit_code = inference_slice_->quit_code();
        const auto &runners = inference_slice_->runners();
        const auto &nodes = inference_slice_->nodes();

        // 1. title line
        if (tab_first) { oss << std::setw(tab_space) << ""; }
        oss << "status: " << std::setw(7);
        if (is_running()) {
            oss << "running";
        } else {
            oss << "stopped"
                << ", (exit code: " << quit_code << ")";
        }
        oss << ", " << name_ << std::endl;

        // 2. runners
        oss << std::setw(tab_space) << "";
        oss << " runners: " << runners.size();
        if (!runners.empty()) {
            int run_idx = 0;
            oss << " {";
            for (const auto &r : runners) {
                oss << r.first;
                run_idx++;
                if (run_idx < (int)runners.size()) { oss << ", "; }
            }
            oss << "}";
        }
        oss << std::endl;

        // 3. nodes
        oss << std::setw(tab_space) << "";
        oss << " nodes: " << nodes.size() << std::endl;
        for (const auto &n : nodes) {
            oss << std::setw(tab_space) << "";
            oss << "  * node: " << std::setw(2) << n.first << ", " << n.second->name() << std::endl;
        }
        return oss;
    }

    std::string get_runtime_json() const {
        std::vector<std::shared_ptr<gddi::ngraph::NodeAny>> all_nodes;

        if (inference_slice_) {
            for (const auto &n : inference_slice_->nodes()) { all_nodes.push_back(n.second); }
        }
        if (preview_slice_) {
            for (const auto &n : preview_slice_->nodes()) { all_nodes.push_back(n.second); }
        }

        return dump_runtime_json(all_nodes);
    }
};

#endif//FFMPEG_WRAPPER_SRC__INFERENCE_SERVER___INFERENCE_TASK_HPP_
