// gateway.h
#pragma once

#include "port_scanner/port_scanner.h"

#include <cpp-http/httplib.h> // http 客户端支持

namespace module {
using Logger = std::shared_ptr<SimpleLoggerInterface>; // 日志接口
class Gateway {
public:
    struct Config {
        std::string host;
        uint16_t port;
    };

private:
    Config config_;
    std::unique_ptr<httplib::Server> server_;
    Logger logger_;

public:
    Gateway(const Config &config);
    ~Gateway();

    Gateway &set_logger(Logger logger);

    Gateway &register_proxy_get_api(std::string_view target, httplib::Server::Handler handler);

    void run();
    void stop();

private:
    bool validate_ipv4_address(std::string_view address);
};

} // namespace module
