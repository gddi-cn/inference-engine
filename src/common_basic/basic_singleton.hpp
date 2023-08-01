//
// Created by cc on 2021/12/24.
//

#ifndef CPX_SRC_BASIC_SINGLETON_HPP_
#define CPX_SRC_BASIC_SINGLETON_HPP_

#include <memory>
#include <mutex>

namespace gddi {
template<class Ty_>
class basic_singleton {
public:
    static Ty_ &instance() {
        static std::shared_ptr<Ty_> instance_;
        static std::mutex mutex_;

        std::lock_guard<std::mutex> lock_guard(mutex_);

        if (instance_.get() == nullptr) {
            instance_ = std::shared_ptr<Ty_>(new Ty_());
        }

        return *instance_.get();
    }

protected:
    basic_singleton() = default;
    ~basic_singleton() = default;
};
}

#endif //CPX_SRC_BASIC_SINGLETON_HPP_
