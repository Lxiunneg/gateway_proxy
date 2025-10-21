// port_scanner.h
#pragma once

/**
 * ************************************************************************
 * @brief Windows 平台端口扫描器实现，周期性的扫描 [begin,end] 的端口的占用情况
 *
 *
 * ************************************************************************
 */

#include "simple_log/simple_log.h"

#include <string_view>
#include <stdint.h>
#include <format> // MSVC C++20
#include <stdexcept>
#include <set>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <shared_mutex>
#include <sstream>

namespace module {

class PortScanner {
    using Logger = std::shared_ptr<SimpleLoggerInterface>; // 日志接口

public:
    struct Config {
        uint16_t base = 0;        // 基准端口 （设备号 = 端口 - 基准端口）
        uint16_t begin = 0;       // 监视起始端口
        uint16_t end = 65535;     // 监视结束端口
        uint16_t interval = 5000; // ms
        void validate() {
            if (end < begin) {
                throw std::invalid_argument(std::format("end[{}] < begin[{}]", end, begin));
            }

            if (interval < 5000) {
                interval = 5000;
            }
        };
    };

private:
    // 端口占用查询指令 只查询 TCP-IPV4-LISTEN 端口
    static constexpr std::string_view CMD = "netstat -ano | findstr TCP | findstr 0.0.0.0  | findstr LISTEN";

    Config config_;
    std::set<uint16_t> using_ports_;

    // 日志
    Logger logger_;

    // 线程安全
    std::atomic<bool> running_;
    std::thread worker_;
    std::mutex worker_mtx_;
    std::condition_variable worker_cv_;
    std::shared_mutex data_mtx_;

public:
    PortScanner(const Config &config);
    ~PortScanner();

    PortScanner &set_logger(Logger logger) {
        logger_ = logger;
        return *this;
    }

    void run();
    void stop();

    std::set<uint16_t> get_using_ports();

private:
    /// @brief 工作任务
    void work();

    /// @brief 调用系统函数，执行指令
    /// @param cmd 指令
    /// @return 执行结果集
    std::string exec_cmd(std::string_view cmd);

    /// @brief 解析一行结果
    /// @param line 一行结果
    /// @return 端口
    uint16_t extract_port_from_line(const std::string &line);

    /// @brief 格式化打印集合
    /// @param ports 端口集合
    /// @return 格式化集合字符串
    std::string print_ports(const std::set<uint16_t> &ports);
};

} // namespace module
