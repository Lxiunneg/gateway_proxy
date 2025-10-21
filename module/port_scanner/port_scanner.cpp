// port_scanner.cpp
#include "port_scanner.h"

#include <array>
#include <algorithm>

using namespace module;

PortScanner::PortScanner(const Config &config) : config_(config),
                                                 running_(false) {
    config_.validate();
}

PortScanner::~PortScanner() {
    stop();
}

void PortScanner::run() {
    running_.store(true);
    worker_ = std::thread([this]() {
        while (running_.load()) {
            std::unique_lock<std::mutex> lock(worker_mtx_);
            auto next_run = std::chrono::steady_clock::now() + std::chrono::milliseconds(config_.interval);

            work();

            auto is_stop = worker_cv_.wait_until(lock, next_run, [this]() {
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
    worker_cv_.notify_one();
    if (worker_.joinable())
        worker_.join();
}

std::set<uint16_t> PortScanner::get_using_ports() {
    std::shared_lock read_lock(data_mtx_);
    return using_ports_;
}

void PortScanner::work() {
    auto result = exec_cmd(CMD);
    std::set<uint16_t> current_using_ports;

    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
        uint16_t port = extract_port_from_line(line);
        if (config_.begin <= port && port <= config_.end)
            current_using_ports.insert(port);
    }

    // 并发安全
    {
        std::unique_lock write_lock(data_mtx_);

        // 计算新增与关闭
        std::set<uint16_t> add_ports;
        std::set<uint16_t> closed_ports;

        // 计算新增
        std::set_difference(
            current_using_ports.begin(), current_using_ports.end(),
            using_ports_.begin(), using_ports_.end(),
            std::inserter(add_ports, add_ports.begin()));

        if (!add_ports.empty()) {
            logger_->info(std::format("监测的服务新增: {}", print_ports(add_ports)));
        }

        // 计算关闭
        std::set_difference(
            using_ports_.begin(), using_ports_.end(),
            current_using_ports.begin(), current_using_ports.end(),
            std::inserter(closed_ports, closed_ports.begin()));

        if (!closed_ports.empty()) {
            logger_->info(std::format("监测的服务关闭: {}", print_ports(closed_ports)));
        }

        // 更新占用情况
        if (using_ports_ != current_using_ports) {
            using_ports_ = current_using_ports;
            logger_->info(std::format("目前发现的服务: {}", print_ports(using_ports_)));
        }
    }
}

std::string PortScanner::exec_cmd(std::string_view cmd) {
    std::array<char, 512> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.data(), "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

uint16_t PortScanner::extract_port_from_line(const std::string &line) {
    // line 示例：TCP    0.0.0.0:53             0.0.0.0:0              LISTENING       56312
    size_t pos = line.find(':'); // 找到第一个 : 符号的位置
    if (pos == std::string::npos) return 0;

    // 从端口号开始，一直找到空格 (读取端口数字)
    size_t start = pos + 1;
    size_t end = line.find(' ', start);
    std::string port_str = (end == std::string::npos) ?
                               line.substr(start) :
                               line.substr(start, end - start);

    try {
        // 字符转数字
        uint16_t port = static_cast<uint16_t>(std::stoi(port_str));
        return port;
    } catch (...) {
        return 0;
    }
}

std::string PortScanner::print_ports(const std::set<uint16_t> &ports) {
    std::stringstream ss;
    ss << "[";
    for (auto it = ports.cbegin(); it != ports.cend(); ++it) {
        ss << *it;
        if (std::next(it) != ports.cend()) {
            ss << ",";
        }
    }
    ss << "]";
    return ss.str();
}