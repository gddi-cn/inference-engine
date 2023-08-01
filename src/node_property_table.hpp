//
// Created by cc on 2021/11/16.
//

#ifndef FFMPEG_WRAPPER_SRC_NODE_PROPERTY_TABLE_HPP_
#define FFMPEG_WRAPPER_SRC_NODE_PROPERTY_TABLE_HPP_

#include <mutex>
#include <memory>
#include <unordered_map>
#include <typeinfo>
#include "utils.hpp"
#include "json.hpp"
#include "basic_logs.hpp"

namespace gddi {
namespace ngraph {

enum class PropAccess {
    kPublic,
    kProtected,
    kPrivate
};

struct NodePropertyEntry {
    nlohmann::json local_value;
    std::function<bool(nlohmann::json &&property_value)> setter;
    std::function<nlohmann::json()> get_constraint;
    std::function<nlohmann::json()> get_runtime_value;    

    friend std::ostream &operator<<(std::ostream &os, const NodePropertyEntry &obj) {
        os << obj.get_constraint().dump();
        return os;
    }
};

class NodePropertyTable {
public:
    NodePropertyTable() = default;

    bool empty() const noexcept {
        std::lock_guard<std::mutex> lock_guard(mutex_);
        return property_table_.empty();
    }

    /**
     * @brief 用于简易绑定一些只读的参数项
     * @tparam Ty_
     * @param key
     * @param value
     */
    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value,
            const std::vector<Ty_> &options,
            const int minimum, const int maximum, const std::string &desc = {},
            const PropAccess access = PropAccess::kPublic) {
        auto &prop_ = push_new_property_(key, [key, this, &value] {
            try {
                value = property_table_[key].local_value.get<Ty_>();
            }
            catch (nlohmann::json::exception &ex) {
                std::string error = ex.what();
                throw std::runtime_error(key + ":" + error.substr(error.find_first_of(']') + 1));
            }
        });
        
        // v1.4.x only get prop constraint 
        prop_.get_constraint = [key, this, value, options, minimum, maximum, desc, access] {
            nlohmann::json property;

            if (access == PropAccess::kPublic) {
                property["default"] = value;
                property["description"] = desc;

                if (options.size() > 0) {
                    property["enum"] = options;
                } else if (std::is_integral<Ty_>::value && !std::is_same<Ty_, bool>::value) {
                    property["minimum"] = minimum;
                    property["maximum"] = maximum;
                }

                // assign-type
                if (property.count("enum") > 0) {
                    property["type"] = "select";
                } else if (property["default"].is_boolean()) {
                    property["type"] = "boolean";
                } else if (property["default"].is_number()) {
                    property["type"] = "number";
                } else if (property["default"].is_string()) {
                    property["type"] = "string";
                } else {
                    property["type"] = "array";
                }
            }

            return property;
        };
        
        // v1.3.x get runtime value
        prop_.get_runtime_value = [&value] {
            nlohmann::json run_time_value = value;
            return run_time_value;
        };
    }

    template<class Ty_>
    void bind_simple_flags(const std::string &key, const Ty_ &value) {
        feature_flags_[key] = value;
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value,
            const std::vector<Ty_> &options, const std::string &desc = {},
            const PropAccess access = PropAccess::kPublic) {
        bind_simple_property(key, value, options, 0, 0, desc, access);
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value, const PropAccess access = PropAccess::kPublic) {
        bind_simple_property(key, value, {}, 0, 0, "", access);
    }

    template<class Ty_>
    void bind_simple_property(const std::string &key, Ty_ &value, const std::string &desc = {},
                              const PropAccess access = PropAccess::kPublic) {
        bind_simple_property(key, value, {}, 0, 0, desc, access);
    }

    void bind_simple_property(const std::string &key, bool &value, const PropAccess access = PropAccess::kPublic) {
        bind_simple_property(key, value, {}, 0, 0, "", access);
    }

    void bind_simple_property(const std::string &key, int &value, int minimum, int maximum,
                              const std::string &desc = {}, const PropAccess access = PropAccess::kPublic) {
        bind_simple_property(key, value, {}, minimum, maximum, desc, access);
    }

    const std::unordered_map<std::string, NodePropertyEntry> &items() const {
        return property_table_;
    }

    const nlohmann::json &feature_flags() const { return feature_flags_; }

    bool try_set_property(const std::string &key, nlohmann::json property_value) {
        auto iter = property_table_.find(key);
        if (iter != property_table_.end()) {
            return iter->second.setter(std::move(property_value));
        }
        return false;
    }

protected:
    NodePropertyEntry &push_new_property_(
        const std::string &key,
        const std::function<void()> &on_changed) {
        std::lock_guard<std::mutex> lock_guard(mutex_);

        auto &entry = property_table_[key];
        entry.setter = [=, &entry](nlohmann::json &&value) {
            entry.local_value = std::move(value);            
            if (on_changed) {
                on_changed();
            }
            return true;
        };
        return entry;
    }

private:
    mutable std::mutex mutex_; // 访问锁
    std::unordered_map<std::string, NodePropertyEntry> property_table_{};
    nlohmann::json feature_flags_;
};

}
}

#endif //FFMPEG_WRAPPER_SRC_NODE_PROPERTY_TABLE_HPP_
