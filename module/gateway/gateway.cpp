#include "gateway.h"

using namespace module;

Gateway::Gateway(const Config &config) :
    config_(config) {
    if (!validate_ipv4_address(config_.host)) {
        throw std::invalid_argument(std::format("host[{}] 不是一个合法的 IPV4 地址", config_.host));
    }
    server_ = std::make_unique<httplib::Server>();
}

Gateway::~Gateway() {
    stop();
}

Gateway &Gateway::set_logger(Logger logger) {
    logger_ = logger;
    return *this;
}

Gateway &Gateway::register_proxy_get_api(std::string_view target, httplib::Server::Handler handler) {
    logger_->info(std::format("注册代理 {}", target));
    server_->Get(target.data(), handler);
    return *this;
}

void Gateway::run() {
    logger_->info(std::format("反向代理网关正在 0.0.0.0:{} 运行", config_.port));
    server_->listen(config_.host, config_.port);
}

void module::Gateway::stop() {
    logger_->info("反向代理网关 结束运行");
    server_->stop();
}

bool Gateway::validate_ipv4_address(std::string_view address) {
    // IPv4 地址最多为 "255.255.255.255" 共15个字符
    if (address.empty() || address.size() > 15) {
        return false;
    }

    int num = 0;            // 当前段的数值
    int dots = 0;           // 点号数量
    bool has_digit = false; // 当前段是否有数字（防止 ".." 或 ".0." 这类情况）

    for (size_t i = 0; i < address.size(); ++i) {
        char c = address[i];

        if (c == '.') {
            if (!has_digit) {
                return false; // 点号前必须有数字，防止 ".1.1.1" 或 ".."
            }
            if (dots >= 3) {
                return false; // 最多3个点
            }
            dots++;
            num = 0;
            has_digit = false;
        } else if (std::isdigit(c)) {
            // 检查是否以 '0' 开头且后面还有数字（如 "01"）
            if (num == 0 && has_digit && c == '0') {
                return false; // 不允许前导零
            }
            num = num * 10 + (c - '0');
            if (num > 255) {
                return false; // 每段最大为255
            }
            has_digit = true;
        } else {
            return false; // 只能是数字或点
        }
    }

    // 必须恰好有3个点，且最后一段有数字
    return dots == 3 && has_digit;
}