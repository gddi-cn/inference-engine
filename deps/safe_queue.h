#ifndef __SAFE_QUEUE_H__
#define __SAFE_QUEUE_H__

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

namespace gddi
{
    template <typename T>
    class SafeQueue
    {
    public:
        SafeQueue() {}
        SafeQueue(SafeQueue const &) = delete;
        SafeQueue &operator=(SafeQueue const &) = delete;

        bool empty();
        size_t size();

        void push(T const &);
        void push(T &&);

        T const &front() const;

        void pop();

        T wait_for_data();
        bool try_for_data(T &);

    private:
        std::queue<T> m_que;
        std::mutex m_mtx;
        std::condition_variable m_cv;
    };

    template <typename T>
    bool SafeQueue<T>::empty()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_que.empty();
    }

    template <typename T>
    size_t SafeQueue<T>::size()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_que.size();
    }

    template <typename T>
    T const &SafeQueue<T>::front() const
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        assert(!m_que.emply());
        return m_que.front();
    }

    template <typename T>
    void SafeQueue<T>::push(T const &t)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_que.push(t);
        m_cv.notify_one();
        return;
    }

    template <typename T>
    void SafeQueue<T>::push(T &&t)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_que.push(std::move(t));
        m_cv.notify_one();
        return;
    }

    template <typename T>
    void SafeQueue<T>::pop()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        assert(!m_que.emply());
        return m_que.pop();
    }

    template <typename T>
    T SafeQueue<T>::wait_for_data()
    {
        std::unique_lock<std::mutex> lk(m_mtx);
        m_cv.wait(lk, [&] { return !m_que.empty(); });
        auto t = m_que.front();
        m_que.pop();
        lk.unlock();
        return t;
    }

    template <typename T>
    bool SafeQueue<T>::try_for_data(T &t)
    {
        std::lock_guard<std::mutex> glk(m_mtx);

        if (m_que.size() == 0)
        {
            return false;
        }

        t = m_que.front();
        m_que.pop();

        return true;
    }
}

#endif