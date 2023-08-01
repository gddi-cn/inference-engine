//
// Created by cc on 2021/11/18.
//

#include "node_manager.hpp"
#include "ngraph_app.hpp"
#include "tabulate.hpp"
#include <atomic>

std::ostream &gddi::NodeManager::print_std_term_style(std::ostream &oss) {
    auto old_flags = oss.flags();

    oss << "Total Node Types: " << node_creators_.size() << std::endl;

    tabulate::Table table;
    table.format().hide_border_bottom().hide_border_top().hide_border_right();
    table.add_row({"type", "name", "version", "description(描述)"});

    std::map<std::string, NodeCreatorEntry> ordered_map;
    for (const auto &iter : node_creators_) { ordered_map[iter.first] = iter.second; }
    for (const auto &iter : ordered_map) {
        table.add_row({iter.first, iter.second.name, iter.second.version, iter.second.description});
    }

    table[0].format()
        .font_style({tabulate::FontStyle::bold, tabulate::FontStyle::italic})
        .font_color(tabulate::Color::white)
        .background_color(tabulate::Color::blue)
        .show_border_bottom();
    table[1].format().show_border_top();

    table.print(oss);
    oss << std::endl;
    oss.flags(old_flags);
    return oss;
}

std::vector<std::string> gddi::NodeManager::get_names() const {
    std::vector<std::string> names;
    for (const auto &iter : node_creators_) {
        names.push_back(iter.second.name);
    }
    return names;
}

std::vector<gddi::NodeManager::NodeInfo> gddi::NodeManager::get_infos() const {
    std::vector<NodeInfo> infos;
    for (const auto &iter : node_creators_) {
        infos.push_back({iter.second.name, iter.second.version, iter.second.description});
    }
    return infos;
}

std::shared_ptr<gddi::ngraph::NodeAny> gddi::NodeManager::create(
    const std::string &type,
    const std::shared_ptr<ngraph::Runner> &runner,
    std::string node_name) {
    auto iter = node_creators_.find(type);
    if (iter != node_creators_.end()) {
        return iter->second.node_creator(runner, std::move(node_name));
    }
    return std::shared_ptr<gddi::ngraph::NodeAny>();
}

std::string gddi::NodeManager::node_name_(const std::string &key_str) {
    auto pos = key_str.find_first_of('_');
    if (pos == std::string::npos) {
        return key_str;
    }
    return key_str.substr(0, pos);
}

std::string gddi::NodeManager::guess_version_(const std::string &key_str) {
    auto pos = key_str.find_first_of('_');
    if (pos == std::string::npos) {
        return {};
    }
    return key_str.substr(pos + 1);
}

nlohmann::json gddi::NodeManager::get_node_constraints() {
    auto json = nlohmann::json::array();

    auto runner = std::make_shared<gddi::ngraph::Runner>("default");
    auto enum_features =
        [](gddi::ngraph::endpoint::Type ep_type, nlohmann::json &j, const std::shared_ptr<ngraph::NodeAny> &node) {
            for (int i = 0;; i++) {
                auto feature = node->get_endpoint_features(ep_type, i);
                if (feature.success) {
                    auto feature_json = nlohmann::json();
                    feature_json["index"] = i;
                    feature_json["feature"] = feature.result.data_features;
                    j.push_back(feature_json);
                } else {
                    break;
                }
            }
        };

    for (const auto &iter : node_creators_) {
        auto tmp_node = iter.second.node_creator(runner, "");

        auto json_node = nlohmann::json();
        json_node["type"] = iter.first;
        json_node["name"] = iter.second.name;
        json_node["version"] = iter.second.version;
        json_node["description"] = iter.second.description;

        auto type_in = gddi::ngraph::endpoint::Type::Input;
        auto type_out = gddi::ngraph::endpoint::Type::Output;

        json_node["ep_in"] = nlohmann::json::array();
        json_node["ep_out"] = nlohmann::json::array();

        enum_features(type_in, json_node["ep_in"], tmp_node);
        enum_features(type_out, json_node["ep_out"], tmp_node);

        if (json_node["ep_in"].empty() && json_node["ep_out"].empty()) {
            auto bridge_ep = nlohmann::json();
            bridge_ep["index"] = 0;
            bridge_ep["feature"] = "any";
            json_node["ep_in"].push_back(bridge_ep);
            json_node["ep_out"].push_back(bridge_ep);
        }

        auto props = tmp_node->properties().items();
        auto json_props = nlohmann::json();
        for (const auto &prop : props) {
            json_props[prop.first] = prop.second.get_runtime_value();
        }
        json_node["props"] = json_props;
        json.push_back(json_node);
    }
    return json;
}

nlohmann::json gddi::NodeManager::get_node_constraints_v2() {
    auto all_nodes = nlohmann::json::object();

    auto runner = std::make_shared<gddi::ngraph::Runner>("default");
    auto enum_features =
        [](gddi::ngraph::endpoint::Type ep_type, nlohmann::json &j, const std::shared_ptr<ngraph::NodeAny> &node) {
            for (int i = 0;; i++) {
                auto feature = node->get_endpoint_features(ep_type, i);
                if (feature.success) {
                    auto feature_json = nlohmann::json();
                    feature_json["id"] = i;
                    feature_json["name"] = feature.result.data_features;
                    j.push_back(feature_json);
                } else {
                    break;
                }
            }
        };

    for (const auto &iter : node_creators_) {
        auto tmp_node = iter.second.node_creator(runner, "");

        auto json_node = nlohmann::json();
        
        json_node["name"] = iter.second.name;
        json_node["version"] = iter.second.version;
        json_node["description"] = iter.second.description;

        auto type_in = gddi::ngraph::endpoint::Type::Input;
        auto type_out = gddi::ngraph::endpoint::Type::Output;

        json_node["inputs"] = nlohmann::json::array();
        json_node["outputs"] = nlohmann::json::array();

        enum_features(type_in, json_node["inputs"], tmp_node);
        enum_features(type_out, json_node["outputs"], tmp_node);

        auto props = tmp_node->properties().items();
        for (const auto &prop : props) {
            auto prop_obj = prop.second.get_constraint();
            if (!prop_obj.is_null()) {
                json_node["props"][prop.first] = prop_obj;
            }
        }
        json_node["feature_flags"] = tmp_node->properties().feature_flags();

        all_nodes[iter.second.name + "_" + iter.second.version] = json_node;
    }

    return all_nodes;
}
