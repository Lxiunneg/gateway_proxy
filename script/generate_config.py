import json

if __name__ == "__main__":
    config = {}

    # 端口扫描器 配置项
    port_scanner = {}
    port_scanner["base"] = 5600
    port_scanner["begin"] = 5600
    port_scanner["end"] = 5620
    port_scanner["interval"] = 5000

    # 代理网关配置
    target_1 = {}
    target_1["target"] = "/produce"
    params_1 = ["machine_no", "shift"]
    target_1["params"] = params_1

    gateway_proxy = {}
    gateway_proxy["host"] = "0.0.0.0"
    gateway_proxy["port"] = 11000
    gateway_proxy["targets"] = [target_1]

    config["port_scanner"] = port_scanner
    config["gateway"] = gateway_proxy

    with open("config/config.json", "w", encoding="utf-8") as f:
        json.dump(config, f, indent=4)
