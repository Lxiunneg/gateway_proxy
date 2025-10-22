#include "httplib.h"
#include "my_json/my_json.h"
#include "port_scanner/port_scanner.h"
#include "simple_log/simple_log.h"

#include <fstream>
#include <filesystem>
#include <format>
#include <regex>

#ifdef WIN32
#include <Windows.h>
class CHCP {
public:
    CHCP() {
        SetConsoleOutputCP(65001);
    }
} chcp;
#endif // WIN32

// base
static uint16_t g_base_port{}; // 基准端口
static std::string g_host{};
static uint16_t g_port{};

// scanner
static uint16_t g_begin{};
static uint16_t g_end{};
static uint16_t g_scan_interval{5};

static inline bool load_from_json(std::string_view path) {
    bool is_error{false};

    std::fstream file(path.data());
    if (!file.is_open()) {
        throw std::runtime_error(std::format("无法打开文件! {}", path));
    }
    if (!file.good()) {
        throw std::runtime_error(std::format("无法使用文件! {}", path));
    }

    Json::Value root;
    file >> root;

    try {
        // base
        if (!root.isMember("base")) {
            throw std::invalid_argument("JSON 缺少 base 结构");
        }
        auto &base_root = root["base"];
        g_base_port = module::from_json::get_value<int>(base_root, "base_port");
        g_host = module::from_json::get_value_or<std::string>(base_root, "host", "0.0.0.0");
        g_port = module::from_json::get_value_or<int>(base_root, "port", 11000);

        // scanner
        if (!root.isMember("scanner")) {
            throw std::invalid_argument("JSON 缺少 scanner 结构");
        }
        auto &scanner_root = root["scanner"];
        g_begin = module::from_json::get_value<int>(scanner_root, "begin");
        g_end = module::from_json::get_value<int>(scanner_root, "end");
        g_scan_interval = module::from_json::get_value_or<int>(scanner_root, "scan_interval", 5);
    } catch (const Json::Exception &ex) {
        std::cerr << "解析JSON时发生错误" << ex.what() << std::endl;
        is_error = true;
    } catch (const std::invalid_argument &ex) {
        std::cerr << "加载JSON时发生错误" << ex.what() << std::endl;
        is_error = true;
    }

    return is_error;
}

// 获取程序的绝对路径
static inline std::string get_program_current_path() {
    namespace fs = std::filesystem;
    fs::path executable_path = fs::current_path();
    return executable_path.string();
}

// 服务发现 API 返回结构
struct ServiceSearchRespone {
    std::size_t cnt;
    std::vector<uint16_t> machine_list;
};

// 失败回复
struct ErrorRespone {
    std::string error_msg;
};

int main() {
    try {
        xiunneg::WSADATARAII wsa_data;

        if (load_from_json(std::format("{}\\config\\config.json", get_program_current_path()))) {
            return -1;
        }

        xiunneg::PortScanner::Config config{
            .begin = g_begin,
            .end = g_end,
            .scan_interval = g_scan_interval,
        };
        xiunneg::PortScanner port_scanner(config);
        port_scanner.run();

        httplib::Server proxy_gateway;
        module::SimpleLogger proxy_gateway_logger("proxy_gateway_logger", module::SimpleLoggerInterface::LoggerMode::CONSOLE_AND_FILE);

        // 服务发现
        proxy_gateway.Get("/machine-list", [&port_scanner](const httplib::Request &req, httplib::Response &res) {
            ServiceSearchRespone resp;
            auto services = port_scanner.get_occupied_ports();
            resp.cnt = services.size();
            for (const auto &service_port : services)
                resp.machine_list.push_back(service_port - g_base_port);

            res.set_content(module::to_json::dump(module::to_json::to_json(resp), 4), "application/json");
        });

        // GET 请求转发 必选参数 machine_no[服务号]
        proxy_gateway.Get("/.*", [&](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("machine_no")) {
                ErrorRespone resp;
                resp.error_msg = "缺少参数 machine_no";
                proxy_gateway_logger.error(std::format("{}:{} {}", req.remote_addr, req.remote_port, resp.error_msg));
                res.set_content(module::to_json::dump(module::to_json::to_json(resp), 4), "application/json");
                return;
            }

            int machien_no{};

            try {
                machien_no = std::stoi(req.get_param_value("machine_no"));
            } catch (const std::out_of_range &e) {
                ErrorRespone resp;
                resp.error_msg = std::format("参数 machine_no[{}] 超出范围! {}", req.get_param_value("machine_no"), e.what());
                proxy_gateway_logger.error(std::format("{}:{} {}", req.remote_addr, req.remote_port, resp.error_msg));
                res.set_content(module::to_json::dump(module::to_json::to_json(resp), 4), "application/json");
                return;
            }

            int target_sevice = g_base_port + machien_no;

            // 本地转发
            auto services = port_scanner.get_occupied_ports();
            if (services.find(target_sevice) != services.end()) {
                httplib::Client client("127.0.0.1", target_sevice);
                httplib::Headers headers = req.headers;
                auto client_resp = client.Get(req.target, headers);
                proxy_gateway_logger.info(std::format("{}:{} GET {}", req.remote_addr, req.remote_port, req.target));
                res.set_content(client_resp->body, "application/json");
            } else {
                ErrorRespone resp;
                resp.error_msg = std::format("该服务[{}]不存在!", req.get_param_value("machine_no"));
                proxy_gateway_logger.error(std::format("{}:{} {}", req.remote_addr, req.remote_port, resp.error_msg));
                res.set_content(module::to_json::dump(module::to_json::to_json(resp), 4), "application/json");
                return;
            }
        });

        proxy_gateway.listen(g_host, g_port);
    } catch (const std::exception &e) {
        std::cerr << "程序发生错误: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "程序发生未知错误!" << std::endl;
    }

    return 0;
}