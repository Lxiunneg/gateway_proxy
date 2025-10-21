// main.cpp
#include "port_scanner/port_scanner.h"
#include "gateway/gateway.h"
#include "to_json/to_json.h"

#ifdef WIN32
class CHCP {
public:
    CHCP() {
        SetConsoleOutputCP(65001);
    }
} chcp;
#endif // WIN32

#include <filesystem>



using namespace module;

// 服务发现 API 返回结构
struct ServiceSearchRespone {
    std::size_t cnt;
    std::vector<uint16_t> machine_list;

    void to_machine_no(uint16_t base_port) {
        for (auto &elem : machine_list)
            elem -= base_port;
    }
};

// 读取 json 配置文件
Json::Value load_from_json(std::string_view path) {
    std::fstream file(path.data());

    if (!file.is_open()) {
        throw std::runtime_error(std::format("无法打开文件: {}", path));
    }

    Json::Value root;
    file >> root;

    return root;
}

// 获取程序的绝对路径
std::string get_program_current_path() {
    namespace fs = std::filesystem;
    fs::path executable_path = fs::current_path();
    return executable_path.string();
}

template <typename T>
T get_value(Json::Value &root, std::string_view key) {
    if (!root.isMember(key.data())) {
        throw std::invalid_argument(std::format("Json 文件中缺失 {} 项", key));
    }

    if (!root[key.data()].is<T>()) {
        throw std::invalid_argument(std::format("Json 文件中 {} 类型错误", key));
    }

    return root[key.data()].as<T>();
};

template <typename T>
T get_value_or(Json::Value &root, std::string_view key, T data) {
    try {
        return get_value<T>(root, key);
    } catch (const std::invalid_argument &) {
        return data;
    }
};

int main() {
    try {
        auto root = load_from_json(get_program_current_path() + "\\config\\config.json");

        if (!root.isMember("port_scanner")) {
            throw std::invalid_argument("Json 文件中缺失 port_scanner");
        }

        auto port_scanner_root = root["port_scanner"];

        PortScanner::Config port_scanner_config_{
            .base = static_cast<unsigned short>(get_value<int>(port_scanner_root, "base")),
            .begin = static_cast<unsigned short>(get_value<int>(port_scanner_root, "begin")),
            .end = static_cast<unsigned short>(get_value<int>(port_scanner_root, "end")),
            .interval = static_cast<unsigned short>(get_value_or<int>(port_scanner_root, "interval", 5000)),
        };

        PortScanner port_scanner_(port_scanner_config_);
        auto psc_logger =
            std::make_shared<SimpleLogger>("port_scanner_logger",
                                           SimpleLoggerInterface::LoggerMode::CONSOLE_AND_FILE);

        port_scanner_.set_logger(psc_logger)
            .run();

        if (!root.isMember("gateway")) {
            throw std::invalid_argument("Json 文件中缺失 gateway");
        }

        auto gateway_root = root["gateway"];

        Gateway gateway(Gateway::Config{
            .host = get_value<std::string>(gateway_root, "host"),
            .port = static_cast<unsigned short>(get_value_or<int>(gateway_root, "port", 11000)),
        });
        auto gateway_logger =
            std::make_shared<SimpleLogger>("gateway_logger",
                                           SimpleLoggerInterface::LoggerMode::CONSOLE_AND_FILE);
        gateway.set_logger(gateway_logger);

        // 注册 target
        if (!gateway_root.isMember("targets")) {
            throw std::invalid_argument("Json 文件中 gateway 缺失 targets");
        }

        auto &targets_root = gateway_root["targets"];

        for (auto &elem : targets_root) {
            // 提取配置
            auto target = get_value<std::string>(elem, "target");

            // 构建 Handler 并注册
            gateway.register_proxy_get_api(target, [&](const httplib::Request &req, httplib::Response &res) {
                Json::Value resp_json;
                int machine_no = -1;
                int shift = -1;

                // 必选参数 machine_no
                if (!req.has_param("machine_no")) {
                    // 不包含必选参数,请求不合法
                    resp_json["error_log"] = "不包含必选参数 machine_no,请求非法!";
                    res.set_content(to_json::dump(resp_json, 4), "application/json");
                    return;
                } else {
                    try {
                        machine_no = std::stoi(req.get_param_value("machine_no"));
                    } catch (const std::out_of_range &e) {
                        resp_json["error_log"] = std::format("参数 machine_no,超出范围!{}", e.what());
                        res.set_content(to_json::dump(resp_json, 4), "application/json");
                        return;
                    }
                }

                // 可选参数 shift
                if (req.has_param("shift")) {
                    try {
                        shift = std::stoi(req.get_param_value("shift"));

                        if (!(0 <= shift && shift <= 2)) {
                            throw std::out_of_range("");
                        }

                    } catch (const std::out_of_range &e) {
                        resp_json["error_log"] = std::format("参数 shift ,超出范围[0,2]!{}", e.what());
                        res.set_content(to_json::dump(resp_json, 4), "application/json");
                        return;
                    }
                }

                // 代理发送
                auto using_ports_set = port_scanner_.get_using_ports();
                auto target_port = port_scanner_config_.base + machine_no;
                if (using_ports_set.find(target_port) != using_ports_set.end()) {
                    try {
                        // 因端口信息实时变化，每次使用新的客户端
                        httplib::Client cli("127.0.0.1", target_port); // 本地代理
                        httplib::Headers headers;
                        headers = req.headers; // 头部转发

                        std::string proxy_target = "";
                        auto pos = req.target.find('?');
                        if (pos != std::string::npos) {
                            proxy_target = req.target.substr(0, pos);
                        }

                        if (shift != -1) {
                            // 有选择 shift 参数
                            proxy_target += std::format("?shift={}", shift);
                        }

                        auto proxy_res = cli.Get(proxy_target, headers);

                        res.set_content(proxy_res->body, "application/json");
                        gateway_logger->info(std::format("{}:{} GET [machine_no={}] {}", req.remote_addr, req.remote_port, machine_no, proxy_target));
                    } catch (const std::exception &e) {
                        gateway_logger->error(std::format("{}:{} GET {} 发生错误: {}", req.remote_addr, req.remote_port, req.target ,e.what()));
                    }

                } else {
                    resp_json["error_log"] = std::format("该服务编号 [{}] 不在线!", machine_no);
                    res.set_content(to_json::dump(resp_json, 4), "application/json");
                }
            });
        }

        gateway
            // 服务发现 API
            .register_proxy_get_api("/machine-list", [&port_scanner_, &port_scanner_config_, &gateway_logger](const httplib::Request &req, httplib::Response &res) {
                ServiceSearchRespone response{};

                for (const auto &port : port_scanner_.get_using_ports())
                    response.machine_list.push_back(port);
                response.to_machine_no(port_scanner_config_.base);
                response.cnt = response.machine_list.size();

                res.set_content(to_json::dump(to_json::to_json(response)), "application/json");
                gateway_logger->info(std::format("{}:{} GET /machine-list", req.remote_addr, req.remote_port));
            })
            .run();
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}