#ifndef __GDDI_THREAD__
#define __GDDI_THREAD__

#include <boost/noncopyable.hpp>
#include <functional>
#include <thread>

#ifdef _WIN32
#include <windows.h>
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
struct THREADNAME_INFO {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
};
#pragma pack(pop)
#endif

namespace gddi {

class Thread : public boost::noncopyable {
public:
    enum class DtorAction { join, detach };
    using ThreadCallback = std::function<void()>;

    explicit Thread(const ThreadCallback &cb, const DtorAction action) : cb_(cb), action_(action) {}
    ~Thread() {
        if (action_ == DtorAction::join) {
            if (handle_.joinable()) { handle_.join(); }
        } else {
            handle_.detach();
        }
    }

    void start() { handle_ = std::thread(cb_); }

    bool bind(int cpu_id) {
#ifdef _WIN32
        if (SetThreadAffinityMask(handle_.native_handle(), 1 << cpu_id) == 0) { return false; }
#else
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        if (pthread_setaffinity_np(handle_.native_handle(), sizeof(cpu_set_t), &cpuset) != 0) {
            return false;
        }
#endif
        return true;
    }
    bool rename(const std::string &name) {
#ifdef _WIN32
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name.c_str();
        info.dwThreadID = GetThreadId(handle_.native_handle());
        info.dwFlags = 0;

        __try {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                           (ULONG_PTR *)&info);
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
        if (pthread_setname_np(handle_.native_handle(), name.c_str()) != 0) { return false; }
#endif
        return true;
    }

private:
    DtorAction action_;
    std::thread handle_;
    ThreadCallback cb_;
};

}// namespace gddi

#endif//__GDDI_THREAD__