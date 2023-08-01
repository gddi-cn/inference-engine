/**
 * Created by zhdotcai on 4/21/22.
 * 
 * 设备内存池管理
 **/

#ifndef __MEM_POOL_HPP__
#define __MEM_POOL_HPP__

#include "concurrentqueue.h"
#include <chrono>
#include <functional>
#include <memory>

namespace gddi {

template<typename T, typename U = int64_t>

// 实际分配内存对象
struct MemObject {
    explicit MemObject(std::function<void(T *)> on_free, const U &u) {
        data = std::shared_ptr<T>(new T, [on_free, u](T *ptr) {
            if (on_free) { on_free(ptr); };
            delete ptr;
        });
    }
    std::shared_ptr<T> data;
};

template<typename T, typename... Args>
class MemPool : public std::enable_shared_from_this<MemPool<T, Args...>> {
public:
    using MemAlloc = std::function<void(T *, Args...)>;
    using MemFree = std::function<void(T *)>;

public:
    MemPool() { queue_ = std::make_shared<moodycamel::ConcurrentQueue<std::shared_ptr<T>>>(); }

    // 对象申请，不管理
    std::shared_ptr<MemObject<T>> alloc_mem_detach(Args... args) {
        auto mem_object = std::make_shared<MemObject<T>>(mem_free_, 0);
        if (mem_alloc_) { mem_alloc_(mem_object->data.get(), args...); }
        return mem_object;
    }

    // 对象申请，不管理，与 typename U 同生命周期
    template<typename U>
    std::shared_ptr<MemObject<T, U>> alloc_mem_detach(const U &u, Args... args) {
        auto mem_object = std::make_shared<MemObject<T, U>>(mem_free_, u);
        if (mem_alloc_) { mem_alloc_(mem_object->data.get(), args...); }
        return mem_object;
    }

    // 对象内存管理，队列有则取，没有则申请
    std::shared_ptr<MemObject<T>> alloc_mem_attach(Args... args) {
        auto mem_object = std::shared_ptr<MemObject<T>>(
            new MemObject<T>(mem_free_, 0), [queue = this->queue_](MemObject<T> *ptr) {
                queue->enqueue(ptr->data);
                delete ptr;
            });
        if (!queue_->try_dequeue(mem_object->data)) {
            if (mem_alloc_) { mem_alloc_(mem_object->data.get(), args...); }
        }

        return mem_object;
    }

    // 对象内存管理，与 typename U 同生命周期
    template<typename U>
    std::shared_ptr<MemObject<T, U>> alloc_mem_attach(const U &u, Args... args) {
        auto mem_object = std::shared_ptr<MemObject<T, U>>(
            new MemObject<T, U>(mem_free_, u), [queue = this->queue_](MemObject<T, U> *ptr) {
                queue->enqueue(ptr->data);
                delete ptr;
            });
        if (!queue_->try_dequeue(mem_object->data)) {
            if (mem_alloc_) { mem_alloc_(mem_object->data.get(), args...); }
        }

        return mem_object;
    }

    // 注册申请回调，分配或者初始化设备内存
    void register_alloc_callback(MemAlloc mem_alloc) { mem_alloc_ = mem_alloc; }

    // 注册析构回调，释放设备内存
    void register_free_callback(MemFree mem_free) { mem_free_ = mem_free; }

private:
    std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<T>>> queue_;

    MemAlloc mem_alloc_;
    MemFree mem_free_;
};

}// namespace gddi

#endif//__MEM_POOL_HPP__
