// port_scanner.h
#include "port_scanner.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <stdexcept>
#include <sstream>

// Windows API
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX // windows min() max() 冲突
#include <iostream>
#include <ws2tcpip.h>

#include <Windows.h>
#include <psapi.h> //GetModuleFileNameEx

#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace xiunneg;

xiunneg::WSADATARAII::WSADATARAII() {
    std::call_once(init, []() {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            throw std::runtime_error("WSADATA Failure");
        }
    });
}

xiunneg::WSADATARAII::~WSADATARAII() {
    std::call_once(close, []() {
        WSACleanup();
    });
}

void PortScanner::Config::validate() {
    if (end < begin) {
        auto error_log = std::format("端口扫描器参数错误 end[{}] < begin[{}]", end, begin);
        throw std::runtime_error(error_log);
    }

    if (scan_interval < 5) {
        // 最短为 5s
        scan_interval = 5;
    }
}

PortScanner::PortScanner(const Config &config, Logger logger) :
    config_(config),
    running_(false),
    logger_(logger) {
    // 使用默认日志器
    if (logger_ == nullptr) {
        logger_ = std::make_shared<module::SimpleLogger>(
            "port scanner logger",
            module::SimpleLoggerInterface::LoggerMode::CONSOLE_AND_FILE);
    }
    // 参数校验
    config_.validate();
}

PortScanner::~PortScanner() {
    stop();
}

void PortScanner::run() {
    running_.store(true);
    worker_ = std::thread([this]() {
        while (running_.load()) {
            std::unique_lock<std::mutex> timer_lock(timer_mtx_);
            auto next_run = std::chrono::steady_clock::now() + std::chrono::seconds(config_.scan_interval);

            // do work
            work();

            auto is_stop = timer_cv_.wait_until(timer_lock, next_run, [this]() {
                return !running_.load();
            });

            if (is_stop) {
                break;
            }
        }
    });
}

void PortScanner::stop() {
    running_.store(false);
    timer_cv_.notify_one();
    if (worker_.joinable())
        worker_.join();
}

std::set<uint16_t> xiunneg::PortScanner::get_occupied_ports() {
    return occupied_ports;
}

void PortScanner::work() {
    try {
        scan_port();
    } catch (const std::runtime_error &e) {
        logger_->error(std::string("PortScanner::work ").append(e.what()));
    } catch (...) {
        logger_->error("PortScanner::work 未知异常");
    }
}

void PortScanner::scan_port() {
    std::set<uint16_t> current_occupied_ports;

    ULONG ul_size = sizeof(MIB_TCPTABLE2);
    PMIB_TCPTABLE2 p_tcp_table = (PMIB_TCPTABLE2)malloc(ul_size);

    if (p_tcp_table == nullptr)
        throw std::runtime_error("PortScanner::scan_port 内存不足,申请失败!");

    // 调用时若内存不足，则释放，再次申请
    if (GetTcpTable2(p_tcp_table, &ul_size, TRUE) == ERROR_INSUFFICIENT_BUFFER) {
        free(p_tcp_table);
        p_tcp_table = (PMIB_TCPTABLE2)malloc(ul_size);
        if (p_tcp_table == nullptr)
            throw std::runtime_error("PortScanner::scan_port 内存不足,申请失败!");
    }

    if (GetTcpTable2(p_tcp_table, &ul_size, TRUE) == NO_ERROR) {
        // 遍历端口
        for (DWORD i = 0; i < p_tcp_table->dwNumEntries; ++i) {
            auto local_port = ntohs((u_short)p_tcp_table->table[i].dwLocalPort);
            // 在区间内
            if (config_.begin <= local_port
                && local_port <= config_.end
                && (p_tcp_table->table[i].dwState == MIB_TCP_STATE_LISTEN
                    || p_tcp_table->table[i].dwState == MIB_TCP_STATE_ESTAB) // 处于监听状态或连接状态
            ) {
                current_occupied_ports.insert(local_port);
            }
        }
    }

    // 手动管理
    free(p_tcp_table);

    // 计算新增和关闭
    {
        std::shared_lock read_lock(data_mtx_);

        // 有端口更新
        if (current_occupied_ports != occupied_ports) {
            // 新增
            std::set<uint16_t> add_ports;
            std::set_difference(
                current_occupied_ports.begin(), current_occupied_ports.end(),
                occupied_ports.begin(), occupied_ports.end(),
                std::inserter(add_ports, add_ports.begin()));

            logger_->info(std::format("监测到服务新增: {}", print_set(add_ports)));

            // 关闭
            if (current_occupied_ports != occupied_ports) {
                std::set<uint16_t> closed_ports;
                std::set_difference(
                    occupied_ports.begin(), occupied_ports.end(),
                    current_occupied_ports.begin(), current_occupied_ports.end(),
                    std::inserter(closed_ports, closed_ports.begin()));

                logger_->info(std::format("监测到服务关闭: {}", print_set(closed_ports)));
            }
        }
    }

    // 刷新
    {
        std::unique_lock write_lock(data_mtx_);

        if (current_occupied_ports != occupied_ports) {
            logger_->info(std::format("监测到服务集合: {}", print_set(current_occupied_ports)));
            occupied_ports = current_occupied_ports;
        }
    }
}

std::string PortScanner::print_set(const std::set<uint16_t> &set) {
    if (set.empty()) return "{}";

    std::ostringstream oss;
    oss << '{';
    for (auto it = set.begin(); it != set.end(); ++it) {
        if (it != set.begin()) oss << ',';
        oss << *it;
    }
    oss << '}';
    return oss.str();
}
