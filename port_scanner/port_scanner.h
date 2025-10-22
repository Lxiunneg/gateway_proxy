// port_scanner.h
#pragma once

/**
 * ************************************************************************
 *
 * @file port_scanner.h
 * @author xiunneg
 * @brief  仅支持 Windows 平台,基于 Windows API 的指定端口区间扫描器
 * ************************************************************************
 * @copyright Copyright (c) 2025 xiunneg
 * ************************************************************************
 */

#include "simple_log/simple_log.h"

#include <stdint.h>
#include <set>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <condition_variable>

namespace xiunneg {

class WSADATARAII {
private:
    static inline std::once_flag init;
    static inline std::once_flag close;

public:
    WSADATARAII();
    ~WSADATARAII();
};

class PortScanner {
    using Logger = std::shared_ptr<module::SimpleLoggerInterface>;

public:
    struct Config {
        // port 0 ~ 65535
        uint16_t begin;
        uint16_t end;

        uint16_t scan_interval = 5; // 扫描周期,秒

        void validate();
    };

private:
    Config config_;

    std::set<uint16_t> occupied_ports; // 正在被占用的端口集合

    std::thread worker_;
    std::atomic<bool> running_;
    std::mutex timer_mtx_;
    std::condition_variable timer_cv_;
    std::shared_mutex data_mtx_;

    Logger logger_;

public:
    PortScanner(const Config &config, Logger logger = nullptr);
    ~PortScanner();

    void run();
    void stop();

    std::set<uint16_t> get_occupied_ports();

private:
    void work();
    void scan_port();
    std::string print_set(const std::set<uint16_t> &set);
};
} // namespace xiunneg
