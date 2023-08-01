//
// Created by cc on 2022/1/29.
//

#ifndef INFERENCE_ENGINE_SRC__INFERENCE_SERVER__HELPER_INFERENCE_HELPER_HPP_
#define INFERENCE_ENGINE_SRC__INFERENCE_SERVER__HELPER_INFERENCE_HELPER_HPP_
#include <json.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <stdexcept>
#include <map>

inline bool resource_exist(const std::string &template_name, const std::string &path_ex, std::string &resource_path) {
    namespace fs = boost::filesystem;
    fs::path data_dir(fs::current_path());
    data_dir /= path_ex;
    data_dir /= template_name;
#ifdef DEBUG
    spdlog::debug("test resource: {}", data_dir);
#endif
    if (fs::exists(data_dir)) {
        resource_path = data_dir.string();
        return true;
    }
    return false;
}

std::string guess_resource_path(const std::string &template_name) {
    std::string res_path;
    if (resource_exist(template_name, ".", res_path)) { return res_path; }
    if (resource_exist(template_name, "./data", res_path)) { return res_path; }
    if (resource_exist(template_name, "..", res_path)) { return res_path; }
    if (resource_exist(template_name, "../data", res_path)) { return res_path; }
    if (resource_exist(template_name, "../..", res_path)) { return res_path; }
    if (resource_exist(template_name, "../../data", res_path)) { return res_path; }
    if (resource_exist(template_name, "../../..", res_path)) { return res_path; }
    if (resource_exist(template_name, "../../../data", res_path)) { return res_path; }
    if (resource_exist(template_name, "../../../..", res_path)) { return res_path; }
    if (resource_exist(template_name, "../../../../data", res_path)) { return res_path; }
    if (resource_exist(template_name, "./templates", res_path)) { return res_path; }
    throw std::runtime_error(std::string("no such resource: ") + template_name);
}

inline
std::string load_template(const std::string &template_name,
                          const nlohmann::json &args) {
    std::string template_text;
    if (gddi::utils::read_file(guess_resource_path(template_name), template_text)) {
        for (const auto &iter: args.items()) {
            if (iter.value().is_number()) {
                boost::replace_all(template_text, "\"" + iter.key() + "\"", std::to_string(iter.value().get<float>()));
            } else {
                boost::replace_all(template_text, iter.key(), std::string(iter.value()));
            }
        }
        return template_text;
    }
    throw std::runtime_error(std::string("can not open file: ") + template_name);
}

#endif //INFERENCE_ENGINE_SRC__INFERENCE_SERVER__HELPER_INFERENCE_HELPER_HPP_
