#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#ifdef _WIN32
using MdnsSocket = std::uintptr_t;
inline constexpr MdnsSocket InvalidMdnsSocket = ~MdnsSocket{0};
#else
using MdnsSocket = int;
inline constexpr MdnsSocket InvalidMdnsSocket = -1;
#endif

class MdnsResponder {
public:
    ~MdnsResponder();

    bool start(int service_port);
    void stop();
    bool running() const { return running_; }

private:
    void run_loop();

    std::atomic<bool> running_{false};
    std::thread thread_;
    MdnsSocket socket_fd_ = InvalidMdnsSocket;
#ifdef _WIN32
    bool socket_runtime_started_ = false;
#endif
    int service_port_ = 0;
    std::string host_name_;
};
