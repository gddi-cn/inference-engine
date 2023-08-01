#include "benchmark_utils.hpp"
#include <atomic>

class ReadyStatus {
public:
    ReadyStatus() : status_(false) {}
    void wait_status() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return status_; });
    }

    void set_status() {
        {
            std::lock_guard<std::mutex> lock_guard(mutex_);
            status_ = true;
        }
        cv_.notify_all();
    }

    bool get_status() const {
        std::lock_guard<std::mutex> lock_guard(mutex_);
        return status_;
    }

private:
    bool status_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

// "tcp://127.0.0.1:5678"
// "inproc://benchmark"
#ifdef WITH_CPPZMQ
void bench_zmq_inproc(int message_size, int message_test_total, const std::string &sock_url = "inproc://benchmark") {
    zmq::context_t ctx;
    ReadyStatus wait_ready;
    ReadyStatus sender_finished;
    

    std::cout << "sock_url: " << sock_url << std::endl;
    ConsumerProducerTimeUse time_used = benchmark_consumer_producer([&] {
        wait_ready.wait_status();
        std::cout << "Pub is ready! : " << sock_url << std::endl;
        zmq::socket_t sock(ctx, zmq::socket_type::sub);
        sock.set(zmq::sockopt::subscribe, zmq::const_buffer("", 0));
        sock.connect(sock_url);
        zmq::message_t message;
        std::cout << "Sub connected!\n";

        int32_t test_index_ = 0;
        while (test_index_ < message_test_total) {
            auto res = sock.recv(message, zmq::recv_flags::dontwait);
            
            // if (test_index_ % 1000 == 0)
            //     std::cout << "Sub recv: " << test_index_ << std::endl;
            test_index_++;
        }
    }, [&] {
        zmq::socket_t sock(ctx, zmq::socket_type::pub);
        sock.bind(sock_url);
        const std::string last_endpoint = sock.get(zmq::sockopt::last_endpoint);
        std::cout << "last_endpoint : " << last_endpoint << std::endl;
        wait_ready.set_status();

        int32_t test_index_ = 0;
        while (test_index_ < message_test_total) {
            sock.send(zmq::message_t(message_size), zmq::send_flags::none);
            // std::cout << "Pub send: " << test_index_ << std::endl;
            test_index_++;
        }
    });

    std::cout.imbue(std::locale(""));
    auto oldflags = std::cout.flags();
    std::cout << std::showpoint
              << "producer total: " << message_test_total << std::endl
              << "producer time: " << time_used.producer_time_used
              << "us, " << message_test_total * 1000LL * 1000LL / time_used.producer_time_used << " msg/s"
              << std::endl
              << "consumer time: " << time_used.consumer_time_used
              << "us, " << message_test_total * 1000LL * 1000LL / time_used.consumer_time_used << " msg/s"
              << std::endl;
    std::cout.flags(oldflags);
}
#else
void bench_zmq_inproc(int message_size, int message_test_total, const std::string &sock_url = "inproc://benchmark") {}
#endif

int main(int argc, char *argv[]) {
    bench_zmq_inproc(256, 1000000);

    blocking_concurrent_test<256, 1000000>();

    blocking_concurrent_test<512, 1000000>();

    // blocking_concurrent_test<1024, 1000000>();

    // blocking_concurrent_test<1024 * 1024, 1000>();

    // blocking_concurrent_test<2560 * 1440 * 3, 1000>();
    return 0;
}